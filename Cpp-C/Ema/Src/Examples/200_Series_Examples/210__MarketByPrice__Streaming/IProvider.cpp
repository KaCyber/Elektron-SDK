///*|-----------------------------------------------------------------------------
// *|            This source code is provided under the Apache 2.0 license      --
// *|  and is provided AS IS with no warranty or guarantee of fit for purpose.  --
// *|                See the project's LICENSE.md for details.                  --
// *|           Copyright Thomson Reuters 2016. All rights reserved.            --
///*|-----------------------------------------------------------------------------

#include "IProvider.h"

/*
assumptions:
- dependence on configuration from the EmaConfig.xml file
- server's port from the configuration file.
- source directory refresh message from the configuration file
- standard RDM file dictionary located in the same directory as the app
- directory requests processed by the EMA
- dictionary requests processed by the EMA

this interactive provider shows the following functionality
- accepting login requests in the OMM way
- responding to the marketbyprice streaming requests
- all other domains are status closed
*/

using namespace thomsonreuters::ema::access;
using namespace thomsonreuters::ema::rdm;
using namespace std;

UInt64 itemHandle = 0;

void AppClient::processLoginRequest( const ReqMsg& reqMsg, const OmmProviderEvent& event )
{
	event.getProvider().submit(RefreshMsg().domainType(MMT_LOGIN).name(reqMsg.getName()).nameType(USER_NAME).complete()
		.solicited( true ).state( OmmState::OpenEnum, OmmState::OkEnum, OmmState::NoneEnum, "Login accepted" ),
		event.getHandle() );
}

void AppClient::processMarketByPriceRequest( const ReqMsg& reqMsg, const OmmProviderEvent& event )
{
	if ( itemHandle != 0 )
	{
		processInvalidItemRequest( reqMsg, event );
		return;
	}

	event.getProvider().submit( RefreshMsg().domainType( MMT_MARKET_BY_PRICE ).name( reqMsg.getName() ).serviceName( reqMsg.getServiceName() )
		.state( OmmState::OpenEnum, OmmState::OkEnum, OmmState::NoneEnum, "Refresh Completed" ).solicited( true )
		.payload( Map()
		.summaryData( FieldList().addEnum( 15, 840 ).addEnum( 53, 1 ).addEnum( 3423, 1 ).addEnum( 1709, 2 ).complete() )
		.addKeyAscii( "100", MapEntry::AddEnum, FieldList()
		.addRealFromDouble( 3427, 7.76 )
		.addRealFromDouble( 3429, 9600 )
		.addEnum( 3428, 2 )
		.addRmtes( 3435, EmaBuffer( "Market Maker", 12 ) )
		.complete() )
		.complete() )
		.complete(), event.getHandle() );

	itemHandle = event.getHandle();
}

void AppClient::processInvalidItemRequest( const ReqMsg& reqMsg, const OmmProviderEvent& event )
{
	event.getProvider().submit( StatusMsg().name( reqMsg.getName() ).serviceName( reqMsg.getServiceName() )
		.domainType( reqMsg.getDomainType() )
		.state( OmmState::ClosedEnum, OmmState::SuspectEnum, OmmState::NotFoundEnum, "Item not found" ),
		event.getHandle() );
}

void AppClient::onReqMsg( const ReqMsg& reqMsg, const OmmProviderEvent& event )
{
	switch ( reqMsg.getDomainType() )
	{
	case MMT_LOGIN:
		processLoginRequest( reqMsg, event );
		break;
	case MMT_MARKET_BY_PRICE:
		processMarketByPriceRequest( reqMsg, event );
		break;
	default:
		processInvalidItemRequest( reqMsg, event );
		break;
	}
}

int main( int argc, char* argv[] )
{
	try
	{
		AppClient appClient;
		EmaString OrderNr( "100" );

		OmmProvider provider( OmmIProviderConfig().operationModel( OmmIProviderConfig::UserDispatchEnum ), appClient );

		while ( itemHandle == 0 ) provider.dispatch( 1000 );

		for ( Int32 i = 0; i < 60; i++ )
		{
			unsigned long long  startTime = getCurrentTime();

			provider.dispatch( 10000 );

			provider.submit( UpdateMsg().clear().domainType( MMT_MARKET_BY_PRICE ).payload( Map()
				.addKeyAscii( OrderNr, MapEntry::UpdateEnum, FieldList()
				.addRealFromDouble( 3427, 7.76 + i * 0.1)
				.addRealFromDouble( 3429, 9600)
				.addEnum( 3428, 2)
				.addRmtes( 3435, EmaBuffer( "Market Maker", 12 ) )
				.complete() )
				.complete() ), itemHandle );

			while (getCurrentTime() - startTime < 1000);;
		}
	}
	catch ( const OmmException& excp )
	{
		cout << excp << endl;
	}
	
	return 0;
}
