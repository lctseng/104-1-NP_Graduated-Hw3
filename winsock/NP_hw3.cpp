#include <windows.h>
#include <list>


using namespace std;

#include "resource.h"

#include "np_hw3_lib.h"

#define SERVER_PORT 7799

#define WM_SOCKET_NOTIFY (WM_USER + 1)






//=================================================================
//	Global Variables
//=================================================================


list<WinSocket> Socks;
list<WinSocket> ToDelete;
HWND hwndEdit;




int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
	LPSTR lpCmdLine, int nCmdShow)
{
	
	return DialogBox(hInstance, MAKEINTRESOURCE(ID_MAIN), NULL, MainDlgProc);
}

BOOL CALLBACK MainDlgProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	WSADATA wsaData;

	static SOCKET msock, ssock;
	static WinSocket wsock;
	static struct sockaddr_in sa;

	int err;


	switch(Message) 
	{
		case WM_INITDIALOG:
			hwndEdit = GetDlgItem(hwnd, IDC_RESULT);
			break;
		case WM_COMMAND:
			switch(LOWORD(wParam))
			{
				case ID_LISTEN:

					WSAStartup(MAKEWORD(2, 0), &wsaData);

					//create master socket
					msock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

					if( msock == INVALID_SOCKET ) {
						EditPrintf(hwndEdit, TEXT("=== Error: create socket error ===\r\n"));
						WSACleanup();
						return TRUE;
					}

					err = WSAAsyncSelect(msock, hwnd, WM_SOCKET_NOTIFY, FD_ACCEPT | FD_CLOSE | FD_READ | FD_WRITE);

					if ( err == SOCKET_ERROR ) {
						EditPrintf(hwndEdit, TEXT("=== Error: select error ===\r\n"));
						closesocket(msock);
						WSACleanup();
						return TRUE;
					}

					//fill the address info about server
					sa.sin_family		= AF_INET;
					sa.sin_port			= htons(SERVER_PORT);
					sa.sin_addr.s_addr	= INADDR_ANY;

					//bind socket
					err = bind(msock, (LPSOCKADDR)&sa, sizeof(struct sockaddr));

					if( err == SOCKET_ERROR ) {
						EditPrintf(hwndEdit, TEXT("=== Error: binding error ===\r\n"));
						WSACleanup();
						return FALSE;
					}

					err = listen(msock, 2);
		
					if( err == SOCKET_ERROR ) {
						EditPrintf(hwndEdit, TEXT("=== Error: listen error ===\r\n"));
						WSACleanup();
						return FALSE;
					}
					else {
						EditPrintf(hwndEdit, TEXT("=== Server START ===\r\n"));
					}

					break;
				case ID_EXIT:
					EndDialog(hwnd, 0);
					break;
			};
			break;

		case WM_CLOSE:
			EndDialog(hwnd, 0);
			break;

		case WM_SOCKET_NOTIFY:
			switch( WSAGETSELECTEVENT(lParam) )
			{
				case FD_ACCEPT:
					ssock = accept(msock, NULL, NULL);
					wsock.sock = ssock;
					wsock.msgID = new_message_id();
					// run select
					err = WSAAsyncSelect(wsock.sock, hwnd, wsock.msgID, FD_CLOSE | FD_READ | FD_WRITE);

					if (err == SOCKET_ERROR) {
						EditPrintf(hwndEdit, TEXT("=== Error: select error ===\r\n"));
						closesocket(wsock.sock);
					}


					Socks.push_back(wsock);
					EditPrintf(hwndEdit, TEXT("=== Accept one new client(%d), List size:%d ===\r\n"), ssock, Socks.size());
					break;
				case FD_READ:
					//Write your code for read event here.
					
					break;
				case FD_WRITE:
					//Write your code for write event here

					break;
				case FD_CLOSE:
					break;
			};
			break;
		
		default:
			// other messages, lets check each msgID in socks
			for (WinSocket& wsock : Socks) {
				if (Message == wsock.msgID) {
					string::size_type last_nl;
					int read_n;
					smatch match;
					switch (WSAGETSELECTEVENT(lParam)) {
					case FD_READ:
						read_n = recv(wsock.sock, wsock.read_buf, MAX_BUF_SIZE-1,0);
						wsock.read_buf[read_n] = '\0';
						wsock.read_str += string(wsock.read_buf);
						last_nl = wsock.read_str.rfind("\r\n\r\n");
						if (last_nl != string::npos) {
							// now, whole request is found
							// parse the request line
							if (regex_search(wsock.read_str, match, regex("GET /(.*?)(\\?.*)? (HTTP/\\d\\.\\d)?")))
							{
								string filename = match[1];
								string raw_query = match[2];
								if (regex_search(filename, match, regex("hw3\\.cgi$"))) {
									// CGI
									vector<BatchInfo> batch_list;
									string remain = raw_query;

									// parse query info
									while (regex_search(remain, match, regex("h\\d=([a-zA-Z0-9.-]*)&p\\d=([a-zA-Z0-9.-]*)&f\\d=([a-zA-Z0-9.-]*)&?"))) {
										if (!string(match[1]).empty()) {
											batch_list.emplace_back(match[1], stoi(match[2]), match[3]);
										}
										remain = match.suffix();
									}
									// write heading message
									write_sock_data("HTTP/1.0 200 OK\r\n", wsock.sock);
									write_sock_data("Content-Type: text/html\r\n", wsock.sock);
									write_sock_data("Cache-Control: max-age=0\r\n\r\n", wsock.sock);
									write_sock_data("<html>\n <head>\n <meta http-equiv='Content-Type' content='text/html; charset=big5' />\n <title>Network Programming Homework 3</title>\n </head>\n <body bgcolor=#FFFFFF>\n <font face='Courier New' size=2 color=#FFFF99>\n", wsock.sock);
									write_sock_data("<table width=\"800\" border=\"1\">\n<tr>\n", wsock.sock);
									for (const BatchInfo& info : batch_list) {
										write_sock_data(string("<td>") + info.hostname + string("</td>\r\n"), wsock.sock);
									}
									write_sock_data("</tr><tr>\r\n", wsock.sock);
									
									for (unsigned i = 0;i<batch_list.size();i++) {
										stringstream ss;
										ss << "<td valign=\"top\" id=\"m" << i << "\"></td>\r\n";
										write_sock_data(ss.str(), wsock.sock);
									}
									write_sock_data("</table>", wsock.sock);
									write_sock_data("</font></body></html>", wsock.sock);
									// now, this socket will start as RAS client
									wsock.connect_servers(hwnd,batch_list);

								}
								else {
									// normal file
									// try to open
									EditPrintf(hwndEdit, "Filename: %s, query: %s \n", filename.c_str(), raw_query.c_str());
									ifstream file_in(filename);
									string line;
									if (file_in.is_open()) {
										write_sock_data("HTTP/1.0 200 OK\r\n",wsock.sock);
										write_sock_data("Content-Type: text/html; charset=UTF-8\r\n\r\n", wsock.sock);
										while (getline(file_in, line)) {
											write_sock_data(line + "\r\n", wsock.sock);
										}
									}
									else {
										write_sock_data("HTTP/1.0 404 Not Found\r\n\r\n", wsock.sock);
										write_sock_data("404 Not Found\r\n", wsock.sock);
									}
									wsock.close();
									ToDelete.push_back(wsock);
								}
								
								
								
							}
						}
						break;
					case FD_WRITE:
						// http write
						if (wsock.connected()) {
							if (wsock.p_clients->need_write > 0) {
								EditPrintf(hwndEdit, TEXT("[Notice] Continue Writing HTTP: %s\r\n"), wsock.p_clients->write_buf);
								write_sock_data(wsock.p_clients->write_buf, wsock.sock);
								wsock.p_clients->need_write = 0;
								wsock.p_clients->write_buf[0] = '\0';
							}

						}

						break;
					case FD_CLOSE:
						EditPrintf(hwndEdit, TEXT("[Notice] HTTP connection closed\r\n"));
						break;
					}
				}

			} // end for
			// check for ras clients
			auto it = msg_client_map.find(Message);
			if (it != msg_client_map.end()) {
				// found
				auto& client = *it->second;
				int n_byte;
				switch (WSAGETSELECTEVENT(lParam)) {
				case FD_CONNECT:
					client.state = S_READ;
					EditPrintf(hwndEdit, "[Info] RAS client connected: %s:%d\n", client.info.hostname.c_str(), client.info.port);
					break;
				case FD_READ:
					if(client.state == S_READ){
						memset(client.cli_sock.read_buf, 0, MAX_BUF_SIZE);
						n_byte = recv(client.cli_sock.sock, client.cli_sock.read_buf, MAX_BUF_SIZE,0);
						if (n_byte > 0) {
							client.cli_sock.read_buf[n_byte] = '\0';
							client.inject_string(client.cli_sock.read_buf);
							if (!client.close_read_command) {
								// when get %, go to write mode
								for (int i = 0;i < n_byte;i++) {
									if (client.cli_sock.read_buf[i] == '%') {
										client.cli_sock.read_buf[i] = '\0';
										client.state = S_WRITE;
										client.send_next_command();
										break;
									}
								}
							}
							
						}
						else {
							// closed
							EditPrintf(hwndEdit, "[Info] RAS client disconnected(READ ZERO): %s:%d\n", client.info.hostname.c_str(), client.info.port);
							client.state = S_DONE;
							closesocket(client.cli_sock.sock);


						}
					}

					break;
				case FD_WRITE:
					if (client.state == S_WRITE) {
						if (client.cli_sock.need_write > 0) {
							EditPrintf(hwndEdit, TEXT("[Info] Continue Writing RAS: %s\r\n"), client.cli_sock.write_buf);
							size_t written = write_sock_data(client.cli_sock.write_buf, client.cli_sock.sock);
							client.cli_sock.need_write -= written;
							strncpy(client.cli_sock.write_buf, client.cli_sock.write_buf + written, MAX_BUF_SIZE);
							client.cli_sock.write_buf[written] = '\0';
							if (written == 0) {
								client.state = S_READ;
							}
						}
					}
					break;
				case FD_CLOSE:
					// closed
					EditPrintf(hwndEdit, "[Info] RAS client disconnected(CLOSE): %s:%d\n", client.info.hostname.c_str(), client.info.port);
					client.state = S_DONE;
					// close to ras socket
					closesocket(client.cli_sock.sock);
					--client.collection.conn;
					// check disconnect
					if (client.collection.conn <= 0) {
						EditPrintf(hwndEdit, TEXT("[Notice] command EOF, closing HTTP socket...\r\n"));
						client.collection.p_sock->close();
						ToDelete.push_back(*client.collection.p_sock);
					}
					break;
				}
			}
			if (!ToDelete.empty()) {
				for (const WinSocket& wsock : ToDelete) {
					Socks.remove(wsock);
				}
				ToDelete.clear();
			}
			return FALSE;


	};

	return TRUE;
}

