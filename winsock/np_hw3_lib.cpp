#include "np_hw3_lib.h"

// Winsock
bool WinSocket::operator==(const WinSocket& rhs) const {
	return rhs.msgID == msgID;
}

void WinSocket::close() {
	closesocket(sock);
	//delete p_clients;
	p_clients = nullptr;
}

bool WinSocket::connected()const {
	return p_clients != nullptr;
}

void WinSocket::connect_servers(HWND hwnd,const vector<BatchInfo>& batch_list) {
	p_clients = new NonblockClientCollection(hwnd,this,batch_list);
}

// Batch Info
BatchInfo::BatchInfo(const string& host, int port, const string& filename)
:hostname(host), port(port), filename(filename){}

// NonblockClient
NonblockClient::NonblockClient(NonblockClientCollection& collection,const BatchInfo& info,int index)
:collection(collection),
info(info),
state(S_CONNECT),
close_read_command(false),
index(index),
fin(info.filename)
{
	connect_server();
		
}
void NonblockClient::connect_server() {

	// Temp vars
	struct sockaddr_in sa;
	int err;
	struct hostent *peer_host;
	// Create Socket
	cli_sock.sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (cli_sock.sock == INVALID_SOCKET) {
		EditPrintf(hwndEdit, TEXT("[Error] create RAS socket error\r\n"));
	}
	cli_sock.msgID = new_message_id();
	EditPrintf(hwndEdit, TEXT("[Info] Socket create: %d (msg ID = %u )\r\n"), cli_sock.sock, cli_sock.msgID);

	err = WSAAsyncSelect(cli_sock.sock, collection.hwnd, cli_sock.msgID, FD_CONNECT | FD_CLOSE | FD_READ | FD_WRITE);
	if (err == SOCKET_ERROR) {
		EditPrintf(hwndEdit, TEXT("[Error] RAS socket select error\r\n"));
	}
	// Connect Socket

	// find server addr
	if ((peer_host = gethostbyname(info.hostname.c_str())) == NULL) {
		EditPrintf(hwndEdit, TEXT("[Error] Cannot find RAS server address\r\n"));
	}

	//fill the address info about server
	sa.sin_family = AF_INET;
	sa.sin_port = htons(info.port);
	sa.sin_addr = *((struct in_addr *)peer_host->h_addr);

	//bind socket
	err = connect(cli_sock.sock, (SOCKADDR*)(&sa), sizeof(sa));

	if (err == SOCKET_ERROR) {
		err = WSAGetLastError();
		if (err != WSAEWOULDBLOCK) {
			EditPrintf(hwndEdit, TEXT("[Error] Connect RAS server error , code = %d\r\n"), err);
		}
	}

	// register info map
	auto ent = make_pair(cli_sock.msgID, this);
	msg_client_map.insert(ent);
}

void NonblockClient::inject_string(const char* str)
{
	//EditPrintf(hwndEdit, "[Info] HTTP Inject buffer: %s\n", str);
	const char* ptr = str;
	const char* start = str;
	char buf[MAX_BUF_SIZE];
	stringstream ss(cli_sock.read_str);
	string temp;
	while (*ptr) {
		if (*ptr == '\n') {
			int diff = ptr - start;
			strncpy(buf, start, diff);
			buf[diff] = '\0';
			ss << buf;
			temp = html_format(regex_replace(ss.str(), regex("\r"), ""), index);
			//temp = html_format(ss.str(), index);
			//EditPrintf(hwndEdit, TEXT("[Info] http reponse: %s\r\n"), temp.c_str());
			// write this data to http socket, or store when blocked
			write_http_reponse(temp);
			
			ss.str("");
			start = ptr + 1;

		}
		++ptr;
	}
	if (ptr != start) {
		ss << start;
	}
	cli_sock.read_str = ss.str();
}

void NonblockClient::write_http_reponse(const string & temp)
{
	size_t n_byte;
	n_byte = write_sock_data(temp, collection.p_sock->sock);
	if (n_byte < temp.size()) {
		// some data blocked
		// copy some part info write buffer
		strncpy(collection.write_buf + collection.need_write, temp.c_str() + n_byte, MAX_BUF_SIZE - collection.need_write);
		collection.need_write += min(temp.size() - n_byte, MAX_BUF_SIZE - collection.need_write);
	}
	// else: all send

}

void NonblockClient::send_next_command()
{
	string cmd;
	if (getline(fin, cmd)) {
		
		EditPrintf(hwndEdit, "[Info] Write command %s\n", cmd.c_str());
		size_t n_byte;
		write_http_reponse(html_format(cmd, index, true).c_str());
		cmd += "\r\n";
		n_byte = write_sock_data(cmd, cli_sock.sock);
		if (n_byte < cmd.size()) {
			EditPrintf(hwndEdit, "[Notice] Writing blocked, remain bytes: %d\n",cmd.size() - n_byte);
			// several words blocked
			// copy into write buffer
			strncpy(cli_sock.write_buf + cli_sock.need_write, cmd.c_str() + n_byte, MAX_BUF_SIZE - cli_sock.need_write);
			cli_sock.need_write += min(cmd.size() - n_byte, MAX_BUF_SIZE - cli_sock.need_write);
		}
		else {
			// all written
			state = S_READ;
		}
		// judge exit
		if (regex_search(cmd, regex("^\\s*exit(\\s+|$)"))) {
			close_read_command = true;
		}
	}
	else {
		EditPrintf(hwndEdit, "[Error] EOF when reading command\n");
		close_read_command = true;
	}
}


// NonblockClientCollection
NonblockClientCollection::NonblockClientCollection(HWND hwnd,WinSocket* p_sock,const vector<BatchInfo>& batch_list)
	:hwnd(hwnd),
	p_sock(p_sock),
	conn(batch_list.size())
{
	int index = 0;
	for (const auto& info : batch_list) {
		clients.emplace_back(*this, info,index++);
	}

}


int write_sock_data(const string& str, SOCKET& sock) {
	return send(sock, str.c_str(), str.size(), 0);
}

// append script, transform tag
string html_format(const string& src_str, int index, bool bold) {
	// transform < and >
	// <
	string temp = regex_replace(src_str, regex("<"), "&lt;");
	temp = regex_replace(temp, regex(">"), "&gt;");

	if (bold) {
		temp = string("% <b>") + temp + string("</b>");
	}
	// append header and footer
	stringstream ss;
	ss << "<script>document.all['m" << index << "'].innerHTML += \"" << temp << "<br>\";</script>" << endl;
	return ss.str();
}

int EditPrintf(HWND hwndEdit, TCHAR * szFormat, ...)
{
	TCHAR   szBuffer[1024];
	va_list pArgList;

	va_start(pArgList, szFormat);
	wvsprintf(szBuffer, szFormat, pArgList);
	va_end(pArgList);

	SendMessage(hwndEdit, EM_SETSEL, (WPARAM)-1, (LPARAM)-1);
	SendMessage(hwndEdit, EM_REPLACESEL, FALSE, (LPARAM)szBuffer);
	SendMessage(hwndEdit, EM_SCROLLCARET, 0, 0);
	return SendMessage(hwndEdit, EM_GETLINECOUNT, 0, 0);
}

UINT new_message_id() {
	return ++msgIndex;
}

unordered_map<UINT, NonblockClient*> msg_client_map;
UINT msgIndex = WM_USER + 2;