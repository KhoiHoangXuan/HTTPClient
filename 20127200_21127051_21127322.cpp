#include <iostream>
#include <arpa/inet.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <vector>
#include <string>
#include <sstream>
#include <fstream>

#define bufferSize 1024
// #include <winsock2.h>

using namespace std;

vector<string> getHostAndPathFromURL(string s)
{
    vector<string> v;
    string h;
    int pos = s.find(":");
    h = s.substr(pos + 3);
    int pos2 = h.find("/");
    if (pos2 != string::npos)
    {
        v.push_back(h.substr(0, pos2));
        v.push_back(h.substr(pos2));
    }
    else 
    {
        v.push_back(h);
    }
    return v;
}

vector<string> sepContent(string s, char ss[], int len)
{
    int pos = s.find("\r\n\r\n");
    vector<string> parts;
    parts.push_back(s.substr(0, pos));
    string tmp;
    for (int i = parts[0].length() + 4; i < len; i++)
    {
        tmp += ss[i];
    }
    parts.push_back(tmp);
    return parts;
}

int getContentLength(string s)
{
    stringstream ss(s);
    vector<string> lines;
    string line;
    while(getline(ss, line, '\n'))
    {
        lines.push_back(line);
    }

    for (int i = 0; i < lines.size(); i++)
    {
        int pos = lines[i].find(":");
        string tmp = lines[i].substr(0, pos);
        if (tmp == "Content-Length")
        {
            string tmp2 = lines[i].substr(pos + 2);
            int l = stoi(tmp2);
            return l;
        }

        if (tmp == "Transfer-Encoding")
        {
            string tmp2 = lines[i].substr(pos + 2);
            if (tmp2 == "chunked\r")
            {
                return -1;
            }
        }
    }

    // string tmp = lines[lines.size() - 1].substr(pos + 1);
    // int l = stoi(tmp);
    return -1;
}

void writeFile(string s, fstream &file)
{
    file << s;
}

int hexToDec(string s)
{
    return stoi(s, 0, 16);
}

int hexToDec1(const string& hex) {
    int result;
    stringstream ss;

    for (char c : hex) {
        if (!isxdigit(c)) { 
            cerr << "Invalid hexadecimal character: " << c << endl;
            return -1;
        }
    }

    ss << hex << hex; 
    ss >> result; 
    if (ss.fail()) { 
        cerr << "Hexadecimal to decimal conversion error." << endl;
        return -1;
    }
    return result;
}

string sepChunkedSize(string parts, int &chunkedSize, int valRead)
{
    string chunkedStr;
    int pos = parts.find("\n");
    chunkedStr = parts.substr(0, pos - 1);
    chunkedSize = hexToDec(chunkedStr);
    // parts = parts.substr(pos + 1, valRead);
    // string tmp;
    // for (int i = 0; i < parts.length() - 2; i++)
    // {
    //     tmp += parts[i];
    // }
    return parts.substr(pos + 1, valRead);
}

void handleChunkedResponse(int client_fd, string &bodyContent) {
    //  const int bufferSize = 1024;
    char buffer[bufferSize];
    int n;
    string line;
    
    while (true) {
        line.clear();
        while (true) {
            n = read(client_fd, buffer, 1); 
            if (n < 0) {
                if (errno == EINTR) {
                    continue; 
                } else {
                    cerr << "Read error: " << strerror(errno) << endl;
                    return;
                }
            } else if (n == 0) {
                cerr << "Server closed the connection." << endl;
                return;
            }

            if (buffer[0] == '\n') {
                break; 
            } else if (buffer[0] != '\r') {
                line += buffer[0]; 
            }
        }
        
        int chunkSize = hexToDec1(line); 
        if (chunkSize <= 0) {
            break; 
        }

        int bytesRead = 0;
        while (bytesRead < chunkSize) {
            n = read(client_fd, buffer, min(chunkSize - bytesRead, bufferSize)); 
            if (n < 0) {
                cerr << "Read error: " << strerror(errno) << endl;
                return;
            } else if (n == 0) {
                cerr << "Server closed the connection prematurely." << endl;
                return;
            }
            
            bodyContent.append(buffer, n); 
            bytesRead += n;
        }
        
        
        if (read(client_fd, buffer, 2) != 2) {
            cerr << "Error reading chunk end." << endl;
            return;
        }
    }

}

int main(int argc, char *argv[])
{
    vector<string> v;
    string filename = "test2.txt";
    string host;
    string path;
    if (argc > 1)
    {
        v = getHostAndPathFromURL(argv[1]);
        if (v.size() >= 2)
        {
            path = v[1] + " ";
        }
        else
        {
            path = "";
        }
        host = v[0];
        if (host == "www.google.com.vn")
        {
            path += "/ ";
        }

        cout << "Host: " << host << endl;
        cout << "Path: " << path << endl;
        // if (argv[2] != "")
        filename = argv[2];
    }

    int client_fd;
    if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        cout << "Create socket failed \n";
        return -1;
    }


    struct hostent* server = gethostbyname(host.c_str());
    if (server == nullptr)
    {
        cout << "Cant find\n";
    }

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(80);
    memccpy(&serv_addr.sin_addr.s_addr, server->h_addr_list[0], 0, server->h_length);
    char ip_str[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &serv_addr.sin_addr.s_addr, ip_str, sizeof(ip_str)) != NULL) 
    {
        cout << "IP address: " << ip_str << std::endl;
    }

    // Connect to server
    if (connect(client_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
    {
        cout << "Connection failed \n";
        close(client_fd);
        return -1;
    }

    // Sent request
    string request = "GET ";
    request += path + "HTTP/1.1\r\n";
    request += "Host: " + host + "\r\nConnection: keep-alive\r\nAccept: text/html\r\n\r\n";
    cout << "Request: " << request.c_str() << endl;
    send(client_fd, request.c_str(), request.length(), 0);

    // Get respone
    char buffer[bufferSize * 3];
    int valRead = read(client_fd, buffer, bufferSize * 3);
    // cout << "++++++++++= Buffer: " << valRead << endl;

    vector<string> parts;
    parts = sepContent(buffer, buffer, valRead);


    int c_len = getContentLength(parts[0]);


    if (c_len > 0) // Content-Length
    {
        // cout << "agaga\n";

        int sum = 0;
        
        while(c_len - parts[1].size() + 4 >= bufferSize * 2)
        {

            valRead = read(client_fd, buffer, bufferSize - 1);

            for (int i = 0; i < valRead; i++)
            {
                parts[1] += buffer[i];
            }
            
        }
        valRead = read(client_fd, buffer, c_len - parts[1].size() + 4);
        for (int i = 0; i < valRead; i++)
        {
            parts[1] += buffer[i];
        }
        fstream file;
        file.open(filename, ios::out);
        writeFile(parts[1], file);
        file.close();
        
    }

    else if (c_len == -1) // Chunked 
    {
        // Read the first time
        int chunkedSize;

        fstream file;
        file.open(filename, ios::out);
        parts[1] = sepChunkedSize(parts[1], chunkedSize, valRead);
        // parts[1] += data1;
        parts[1].erase(parts[1].length() - 2, 2);

        // The size of parts[1] > chunkedSize
        if (parts[1].size() > chunkedSize)
        {
            string buffer2 = parts[1].substr(chunkedSize + 2);
            parts[1] = parts[1].substr(0, chunkedSize);


            // Do with buffer2
            string data2 = sepChunkedSize(buffer2, chunkedSize, buffer2.size());




            parts[1] += data2;

        }

        handleChunkedResponse(client_fd, parts[1]);
        writeFile(parts[1], file);
        file.close();
        

    }

    close(client_fd);



    return 0;
}