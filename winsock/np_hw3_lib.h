#pragma once

#include <windows.h>

#include <regex>
#include <sstream>
#include <fstream>
#include <vector>
#include <list>
#include <unordered_map>


#define MAX_BUF_SIZE 16000
#define S_CONNECT 0
#define S_READ 1
#define S_WRITE 2
#define S_DONE 3


using namespace std;





BOOL CALLBACK MainDlgProc(HWND, UINT, WPARAM, LPARAM);
int EditPrintf(HWND, TCHAR *, ...);
UINT new_message_id();
int write_sock_data(const string&, SOCKET&);
string html_format(const string& src_str, int index, bool bold = false);


struct NonblockClientCollection;
struct NonblockClient;


extern unordered_map<UINT, NonblockClient*> msg_client_map;
extern HWND hwndEdit;
extern UINT msgIndex;

struct BatchInfo {
	string hostname;
	int port;
	string filename;

	BatchInfo(const string& host, int port, const string& filename);
};

struct WinSocket {
	SOCKET sock;
	unsigned msgID;
	string read_str;
	char read_buf[MAX_BUF_SIZE+1];
	char write_buf[MAX_BUF_SIZE+1];
	size_t need_write = 0;
	NonblockClientCollection *p_clients = nullptr;

	bool operator==(const WinSocket&) const;
	void close();
	void connect_servers(HWND,const vector<BatchInfo>&);
	bool connected()const;
};



struct NonblockClient {

	WinSocket cli_sock; // socket to ras 
	char write_buf[MAX_BUF_SIZE+1];
	NonblockClientCollection& collection;
	BatchInfo info;
	int state;
	bool close_read_command;
	int index;
	ifstream fin;

	void connect_server();
	void inject_string(const char*);
	void write_http_reponse(const string&);
	void send_next_command();
	NonblockClient(NonblockClientCollection&, const BatchInfo&,int index);

};

struct NonblockClientCollection {

	list<NonblockClient> clients;
	WinSocket* p_sock;

	NonblockClientCollection(HWND,WinSocket*,const vector<BatchInfo>&);
	HWND hwnd;
	char write_buf[MAX_BUF_SIZE + 1];
	size_t need_write = 0;
	int conn;
};


