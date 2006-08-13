/*
 * MEESUT.cpp
 *
 * 2006 Rilson Nascimento
 *
 * 30 July 2006
 */

#include <transactions.h>

char* addr = "localhost";

using namespace TPCE;

CMEESUT::CMEESUT(const int iBHlistenPort, ofstream* pfile)
: m_iBHlistenPort(iBHlistenPort),
  m_pfLog(pfile)
{
	m_fMix.open(MEE_MIX_LOG_NAME, ios::out);
}

CMEESUT::~CMEESUT()
{
	m_fMix.close();
}

// Trade Result
//
void* TPCE::TradeResultAsync(void* data)
{
	PMEESUTThreadParam pThrParam = reinterpret_cast<PMEESUTThreadParam>(data);

	cout<<"Trade Result requested"<<endl;

	PMsgDriverBrokerage pRequest =  new TMsgDriverBrokerage;
	memset(pRequest, 0, sizeof(TMsgDriverBrokerage));

	pRequest->TxnType = TRADE_RESULT;
	memcpy( &(pRequest->TxnInput.TradeResultTxnInput), &(pThrParam->TxnInput.m_TradeResultTxnInput), 
								sizeof( pRequest->TxnInput.TradeResultTxnInput ));

	TMsgBrokerageDriver Reply;	// reply message from BrokerageHouse
	memset(&Reply, 0, sizeof(Reply)); 

	CDateTime	StartTime, EndTime, TxnTime;	// to time the transaction
	CSocket sockdrv;

	try
	{
		// connect to BrokerageHouse
		sockdrv.Connect(addr, pThrParam->pCMEESUT->m_iBHlistenPort);
	
		// record txn start time -- please, see TPC-E specification clause 6.2.1.3
		StartTime.SetToCurrent();
	
		// send and wait for response
		sockdrv.Send(reinterpret_cast<void*>(pRequest), sizeof(TMsgDriverBrokerage));
		sockdrv.Receive( reinterpret_cast<void*>(&Reply), sizeof(Reply));
	
		// record txn end time
		EndTime.SetToCurrent();

		// close connection
		sockdrv.CloseAccSocket();
	
		// calculate txn response time
		TxnTime.Set(0);	// clear time
		TxnTime.Add(0, (int)((EndTime - StartTime) * MsPerSecond));	// add ms
	
		//cout<<"TxnType = "<<TRADE_RESULT<<"\tTxn RT = "<<TxnTime.ToStr(02)<<endl;

		// logging
		pThrParam->pCMEESUT->m_MixLock.ClaimLock();
		if (Reply.iStatus == CBaseTxnErr::SUCCESS)
		{
			pThrParam->pCMEESUT->m_fMix<<(int)time(NULL)<<","<<TRADE_RESULT<< ","<<(TxnTime.MSec()/1000.0)<<
												","<<(int)pthread_self()<<endl;
		}
		else if (Reply.iStatus == CBaseTxnErr::ROLLBACK)
		{
			pThrParam->pCMEESUT->m_fMix<<(int)time(NULL)<<","<<TRADE_RESULT<<"R"<<","<<(TxnTime.MSec()/1000.0)
												<<","<<(int)pthread_self()<<endl;
		}
		else
		{
			pThrParam->pCMEESUT->m_fMix<<(int)time(NULL)<<","<<"E"<<","<<(TxnTime.MSec()/1000.0)
												<<","<<(int)pthread_self()<<endl;
		}
		pThrParam->pCMEESUT->m_fMix.flush();
		pThrParam->pCMEESUT->m_MixLock.ReleaseLock();

	}
	catch(CSocketErr *pErr)
	{
		sockdrv.CloseAccSocket();	// close connection
		pThrParam->pCMEESUT->m_fMix<<(int)time(NULL)<<","<<"E"<<","<<"000"<<","<<(int)pthread_self()<<endl;

		ostringstream osErr;
		osErr<<endl<<"Error: "<<pErr->ErrorText()
		     <<" at "<<"MEESUT::TradeResultAsync"<<endl;
		pThrParam->pCMEESUT->LogErrorMessage(osErr.str());
		delete pErr;
	}

	delete pRequest;
	delete pThrParam;
	return NULL;
}

bool TPCE::RunTradeResultAsync( void* data )
{
	PMEESUTThreadParam pThrParam = reinterpret_cast<PMEESUTThreadParam>(data);

	pthread_t threadID; // thread ID
	pthread_attr_t threadAttribute; // thread attribute

	try
	{
		int status = pthread_attr_init(&threadAttribute); // initialize the attribute object
		if (status != 0)
		{
			throw new CThreadErr( CThreadErr::ERR_THREAD_ATTR_INIT );
		}
	
		// set the detachstate attribute to detached
		status = pthread_attr_setdetachstate(&threadAttribute, PTHREAD_CREATE_DETACHED);
		if (status != 0)
		{
			throw new CThreadErr( CThreadErr::ERR_THREAD_ATTR_DETACH );
		}
	
		// create the thread in the detached state - Call Trade Result asyncronously
		status = pthread_create(&threadID, &threadAttribute, &TradeResultAsync, data);

		if (status != 0)
		{
			throw new CThreadErr( CThreadErr::ERR_THREAD_CREATE );
		}
	}
	catch(CThreadErr *pErr)
	{
		ostringstream osErr;
		osErr<<endl<<"Error: "<<pErr->ErrorText()
		     <<" at "<<"MEESUT::RunTradeResultAsync"<<endl;
		pThrParam->pCMEESUT->LogErrorMessage(osErr.str());
		delete pErr;
		return false;
	}

	// return immediatelly
	return true;	
}

bool CMEESUT::TradeResult( PTradeResultTxnInput pTxnInput )
{
	PMEESUTThreadParam pThrParam = new TMEESUTThreadParam;
	memset(pThrParam, 0, sizeof(TMEESUTThreadParam));

	pThrParam->pCMEESUT = this;
	memcpy(&(pThrParam->TxnInput.m_TradeResultTxnInput), pTxnInput, sizeof(TTradeResultTxnInput));

	return ( RunTradeResultAsync( reinterpret_cast<void*>(pThrParam) ) );
}

// Market Feed
//
void* TPCE::MarketFeedAsync(void* data)
{
	PMEESUTThreadParam pThrParam = reinterpret_cast<PMEESUTThreadParam>(data);

	cout<<"Market Feed requested"<<endl;

	PMsgDriverBrokerage pRequest =  new TMsgDriverBrokerage;
	memset(pRequest, 0, sizeof(TMsgDriverBrokerage));

	pRequest->TxnType = MARKET_FEED;
	memcpy( &(pRequest->TxnInput.MarketFeedTxnInput), &(pThrParam->TxnInput.m_MarketFeedTxnInput), 
								sizeof( pRequest->TxnInput.MarketFeedTxnInput ));

	TMsgBrokerageDriver Reply;	// reply message from BrokerageHouse
	memset(&Reply, 0, sizeof(Reply)); 

	CDateTime	StartTime, EndTime, TxnTime;	// to time the transaction
	CSocket sockdrv;

	try
	{
		// connect to BrokerageHouse
		sockdrv.Connect(addr, pThrParam->pCMEESUT->m_iBHlistenPort);
	
		// record txn start time -- please, see TPC-E specification clause 6.2.1.3
		StartTime.SetToCurrent();
	
		// send and wait for response
		sockdrv.Send(reinterpret_cast<void*>(pRequest), sizeof(TMsgDriverBrokerage));
		sockdrv.Receive( reinterpret_cast<void*>(&Reply), sizeof(Reply));
	
		// record txn end time
		EndTime.SetToCurrent();

		// close connection
		sockdrv.CloseAccSocket();
	
		// calculate txn response time
		TxnTime.Set(0);	// clear time
		TxnTime.Add(0, (int)((EndTime - StartTime) * MsPerSecond));	// add ms
		
		//cout<<"TxnType = "<<MARKET_FEED<<"\tTxn RT = "<<TxnTime.ToStr(02)<<endl;

		// logging
		pThrParam->pCMEESUT->m_MixLock.ClaimLock();
		if (Reply.iStatus == CBaseTxnErr::SUCCESS)
		{
			pThrParam->pCMEESUT->m_fMix<<(int)time(NULL)<<","<<MARKET_FEED<< ","
						<<(TxnTime.MSec()/1000.0)<<","<<(int)pthread_self()<<endl;
		}
		else if (Reply.iStatus == CBaseTxnErr::ROLLBACK)
		{
			pThrParam->pCMEESUT->m_fMix<<(int)time(NULL)<<","<<MARKET_FEED<<"R"<<","
						<<(TxnTime.MSec()/1000.0)<<","<<(int)pthread_self()<<endl;
		}
		else
		{
			pThrParam->pCMEESUT->m_fMix<<(int)time(NULL)<<","<<"E"<<","
						<<(TxnTime.MSec()/1000.0)<<","<<(int)pthread_self()<<endl;
		}
		pThrParam->pCMEESUT->m_fMix.flush();
		pThrParam->pCMEESUT->m_MixLock.ReleaseLock();

	}
	catch(CSocketErr *pErr)
	{
		sockdrv.CloseAccSocket();	// close connection
		pThrParam->pCMEESUT->m_fMix<<(int)time(NULL)<<","<<"E"<<","<<"000"<<","<<(int)pthread_self()<<endl;

		ostringstream osErr;
		osErr<<endl<<"Error: "<<pErr->ErrorText()
		     <<" at "<<"MEESUT::MarketFeedAsync"<<endl;
		pThrParam->pCMEESUT->LogErrorMessage(osErr.str());
		delete pErr;
	}

	delete pRequest;
	delete pThrParam;
	return NULL;
}

bool TPCE::RunMarketFeedAsync(void* data)
{
	PMEESUTThreadParam pThrParam = reinterpret_cast<PMEESUTThreadParam>(data);

	pthread_t threadID; // thread ID
	pthread_attr_t threadAttribute; // thread attribute

	try
	{
		int status = pthread_attr_init(&threadAttribute); // initialize the attribute object
		if (status != 0)
		{
			throw new CThreadErr( CThreadErr::ERR_THREAD_ATTR_INIT );
		}
	
		// set the detachstate attribute to detached
		status = pthread_attr_setdetachstate(&threadAttribute, PTHREAD_CREATE_DETACHED);
		if (status != 0)
		{
			throw new CThreadErr( CThreadErr::ERR_THREAD_ATTR_DETACH );
		}
	
		// create the thread in the detached state - Call Trade Result asyncronously
		status = pthread_create(&threadID, &threadAttribute, &MarketFeedAsync, data);

		if (status != 0)
		{
			throw new CThreadErr( CThreadErr::ERR_THREAD_CREATE );
		}
	}
	catch(CThreadErr *pErr)
	{
		ostringstream osErr;
		osErr<<endl<<"Error: "<<pErr->ErrorText()
		     <<" at "<<"MEESUT::RunMarketFeedAsync"<<endl;
		pThrParam->pCMEESUT->LogErrorMessage(osErr.str());
		delete pErr;
		return false;
	}

	// return immediatelly
	return true;
}

bool CMEESUT::MarketFeed( PMarketFeedTxnInput pTxnInput )
{
	PMEESUTThreadParam pThrParam = new TMEESUTThreadParam;
	memset(pThrParam, 0, sizeof(TMEESUTThreadParam));

	pThrParam->pCMEESUT = this;
	memcpy(&(pThrParam->TxnInput.m_MarketFeedTxnInput), pTxnInput, sizeof(TMarketFeedTxnInput));

	return ( RunMarketFeedAsync( reinterpret_cast<void*>(pThrParam) ) );
}

// LogErrorMessage
void CMEESUT::LogErrorMessage( const string sErr )
{
	m_LogLock.ClaimLock();
	cout<<sErr;
	*(m_pfLog)<<sErr;
	m_pfLog->flush();
	m_LogLock.ReleaseLock();
}