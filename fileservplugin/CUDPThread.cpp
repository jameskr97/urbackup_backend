/*************************************************************************
*    UrBackup - Client/Server backup system
*    Copyright (C) 2011  Martin Raiber
*
*    This program is free software: you can redistribute it and/or modify
*    it under the terms of the GNU General Public License as published by
*    the Free Software Foundation, either version 3 of the License, or
*    (at your option) any later version.
*
*    This program is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*    GNU General Public License for more details.
*
*    You should have received a copy of the GNU General Public License
*    along with this program.  If not, see <http://www.gnu.org/licenses/>.
**************************************************************************/

#pragma warning ( disable:4005 )
#pragma warning ( disable:4996 )

#ifdef _WIN32
#include <winsock2.h>
#endif

#include "../vld.h"
#include "../Interface/Server.h"
#include "CUDPThread.h"
#include "settings.h"
#include "packet_ids.h"
#include "log.h"
#include "FileServ.h"
#include "../stringtools.h"
#include <memory.h>

std::string getServerName(void)
{
#ifdef _WIN32
	char hostname[MAX_PATH];
    _i32 rc=gethostname(hostname, MAX_PATH);

	if(rc!=SOCKET_ERROR)
		return hostname;
	else
		return "_error_";
#else
	char hostname[300];
	_i32 rc=gethostname(hostname,300);
	
	if( rc!=-1 )
		return hostname;
	else
		return "_error_";
#endif
}

CUDPThread::CUDPThread(_u16 udpport,std::string servername)
{
	{
		udpsock=socket(AF_INET,SOCK_DGRAM,0);

		sockaddr_in addr_udp;

		addr_udp.sin_family=AF_INET;
		addr_udp.sin_port=htons(udpport);
		addr_udp.sin_addr.s_addr=INADDR_ANY;

		Log("Binding udp socket...");
		int rc=bind(udpsock, (sockaddr*)&addr_udp, sizeof(sockaddr_in));
		if(rc==SOCKET_ERROR)
		{
#ifdef LOG_SERVER
			Server->Log("Binding udp socket to port "+nconvert(udpport)+" failed", LL_ERROR);
#else
			Log("Failed binding udp socket.");
#endif
			return;
		}
		Log("done.");

#ifdef _WIN32
		DWORD dwBytesReturned = 0;
		BOOL bNewBehavior = FALSE;
		DWORD status;

		Log("Disabling new behavior...");
		// disable  new behavior using
		// IOCTL: SIO_UDP_CONNRESET
		#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR,12)
		status = WSAIoctl(udpsock, SIO_UDP_CONNRESET,
						&bNewBehavior, sizeof(bNewBehavior),
					NULL, 0, &dwBytesReturned,
					NULL, NULL);
		if (SOCKET_ERROR == status)
		{
			Log("Setting SIO_UDP_CONNRESET via WSAIoctl FAILED!");
			//return;
		}
#endif
	}

	if( servername!="" )
		mServername=servername;
	else
		mServername=getServerName();

	Log("Servername: -%s-",mServername.c_str());
}

std::string CUDPThread::getServername()
{
	return mServername;
}

CUDPThread::~CUDPThread()
{
	closesocket(udpsock);
}

void CUDPThread::operator()(void)
{
	Log("UDP Thread startet");
#ifdef _WIN32
	SetThreadPriority( GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
#endif

	while(UdpStep()==true);
	
#ifdef LOG_SERVER
	Server->Log("CUDPThread exited.", LL_ERROR);
#endif
}

bool CUDPThread::UdpStep(void)
{
	fd_set fdset;

	FD_ZERO(&fdset);

	FD_SET(udpsock, &fdset);

	timeval lon;
	memset(&lon,0,sizeof(timeval) );
	lon.tv_sec=60;
	_i32 rc = select((int)udpsock+1, &fdset, 0, 0, &lon);

	if(rc>0)
	{
		Log("Receiving UDP packet...");
		char buffer[BUFFERSIZE];
		socklen_t addrsize=sizeof(sockaddr_in);
		sockaddr_in sender;
		_i32 err = recvfrom(udpsock, buffer, BUFFERSIZE, 0, (sockaddr*)&sender, &addrsize);
		if(err==SOCKET_ERROR)
		{
#ifdef LOG_SERVER
			Server->Log("Recvfrom error in CUDPThread::UdpStep", LL_ERROR);
#endif
			return false;
		}
		else if(err>0)
		{
			if(buffer[0]==ID_PING )
			{
				Log("UDP: PING received... sending PONG");
				char *buffer=new char[mServername.size()+2];
				buffer[0]=ID_PONG;
				buffer[1]=VERSION;
				memcpy(&buffer[2], mServername.c_str(), mServername.size());
				int rc=sendto(udpsock, buffer, (int)mServername.size()+2, 0, (sockaddr*)&sender, sizeof(sockaddr_in) );
				if( rc==SOCKET_ERROR )
				{
					Log("Sending reply failed %i", rc);
				}
				delete[] buffer;
			}
			else
			{
#ifdef LOG_SERVER
				Server->Log("Unknown UDP packet id", LL_WARNING);
#endif
			}
		}
	}
	else if( rc==SOCKET_ERROR )
	{
#ifdef LOG_SERVER
			Server->Log("Select error in CUDPThread::UdpStep", LL_ERROR);
#endif
		return false;
	}

	return true;
}
