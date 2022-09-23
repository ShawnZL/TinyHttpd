#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdlib.h>

#define ISspace(x) isspace((int)(x)) //检查字符是否为空白字符，为字符的ASCII编码，是空白字符返回1true，不是空白字符返回0false

#define SERVER_STRING "Server: jdbhttpd/0.1.0\r\n"

void accept_request(int);//
void bad_request(int); //
void cat(int, FILE *); //
void cannot_execute(int); //
void error_die(const char *); //
void execute_cgi(int, const char *, const char *, const char *); //
int get_line(int, char *, int); //
void headers(int, const char *); //
void unimplemented(int); //
void not_found(int); //
void serve_file(int, const char *); //
/**********************************************************************/
/* A request has caused a call to accept() on the server port to
 * return.  Process the request appropriately.
 * Parameters: the socket connected to the client */
/**********************************************************************/
void accept_request(int client) {
    char buf[1024];
    int numchars;
    char method[255];
    char url[255];
    char path[512];
    size_t i, j;
    struct stat st; //这个结构体是用来描述一个linux系统文件系统中的文件属性的结构。
    int cgi = 0; // becomes true if server decides this is a CGI program
    char *query_string = NULL;
    numchars = get_line(client, buf, sizeof(buf)); //获取的字节数
    /*获取第一行//这边都是在处理第一条http信息
    "GET / HTTP/1.1\n"
     * */
    i = 0; j = 0;
    while (!ISspace(buf[j]) && (i < sizeof(method) - 1)) { //检查非空
        method[i] = buf[j];
        i++;j++;
    }
    //判断是Get还是Post
    if (strcasecmp(method, "GET") && strcasecmp(method, "POST")) {
        // strcasecmp(判断函数s1与s2大小)，s1=s2返回0；此时满足都不等于
        unimplemented(client);
    }
    //如果是post cgi为1
    if (strcasecmp(method, "POST") == 0)
        cgi = 0;
    i = 0;
    // 判断是否空白字符并将空白字符间隔出去
    while (ISspace(buf[j]) && j < sizeof(buf))
        j++;

    // 获取url
    //得到 "/"   注意：如果你的http的网址为http://192.168.0.23:47310/index.html
    //               那么你得到的第一条http信息为GET /index.html HTTP/1.1，那么
    //               解析得到的就是/index.html
    //  get得到的信息就是/ 而post得到的信息就是/index.html
    while (!ISspace(buf[j]) && (i < sizeof(url) - 1) && (j < sizeof(buf))) {
        url[i] = buf[j];
        i++; j++;
    }
    url[i] = '\0';

    // Get 请求
    if (strcasecmp(method, "GET") == 0) {
        query_string = url;
        while ((*query_string != '?') && (*query_string) != '\0')
            query_string++;
        if (*query_string == '?') {
            cgi = 1;
            *query_string = '\0';
            query_string++;
        }
    }
    // 路径，将htdocs+url写入路径当中
    sprintf(path, "htdocs%s", url);

    //默认地址，解析到的路径如果为/，则自动加上index.html
    if (path[strlen(path) - 1] == '/')
        strcat(path, "index.html"); //将函数拼接起来
    //获得文件信息
    if (stat(path, &st) == -1) { //获取文件信息，没有获取到
        //把所有http信息读出然后丢弃
        while ((numchars > 0) && strcmp("\n", buf)) // \n > buf
            // read & discard headers
            numchars = get_line(client, buf, sizeof(buf));
        not_found(client);
    }
    else { //st.st_mode = 文件对应的模式，文件，目录等
        if ((st.st_mode & S_IFMT) == S_IFDIR)//首先S_IFMT是一个掩码，它的值是0170000（注意这里用的是八进制）， 可以用来过滤出前四位表示的文件类型
            /*
             * 现在假设我们要判断一个文件是不是目录，我们怎么做呢？
             * 首先通过掩码S_IFMT把其他无关的部分置0，再与表示目录的数值比较，从而判断这是否是一个目录
             * */
            strcat(path, "/index.html");
        if ((st.st_mode & S_IXUSR) || (st.st_mode & S_IXGRP) || (st.st_mode & S_IXOTH))
            // 此处为判断权限
            /* S_IXUSR owner has execute permission
             * S_IXGRP group has execute permission
             * S_IXOTH others have execute permission
             * */
            cgi = 1;
        if (!cgi)
            serve_file(client, path);
        else
            execute_cgi(client, path, method, query_string);
    }
    close(client);
}

/**********************************************************************/
/* Get a line from a socket, whether the line ends in a newline,
 * carriage return, or a CRLF combination.  Terminates the string read
 * with a null character.  If no newline indicator is found before the
 * end of the buffer, the string is terminated with a null.  If any of
 * the above three line terminators is read, the last character of the
 * string will be a linefeed and the string will be terminated with a
 * null character.
 * Parameters: the socket descriptor
 *             the buffer to save the data in
 *             the size of the buffer
 * Returns: the number of bytes stored (excluding null) */
/**********************************************************************/
// 读取套接字的一行，把回车换行等情况统一换为换行符结束
// \r carriage return回车，\n换行
//得到一行数据,只要发现c为\n,就认为是一行结束，如果读到\r,再用MSG_PEEK的方式读入一个字符，如果是\n，从socket用读出
//如果是下个字符则不处理，将c置为\n，结束。如果读到的数据为0中断，或者小于0，也视为结束，c置为\n
int get_line(int sock, char * buf, int size) {
    int i = 0;
    char c = '\0';
    int n;
    while ((i < size - 1) && (c != '\n')) {
        n = recv(sock, &c, 1, 0); // 1为c参数指向的长度, 0表示取走数据
        // Debug pritnf("%02X\n", c);
        if (n > 0) { //有字节读出
            if (c == '\r') {
                n = recv(sock, &c, 1, MSG_PEEK); // 而当为MSG_PEEK时代表只是查看数据，而不取走数据。
                if ((n > 0) && (c == '\n'))
                    recv(sock, &c, 1, 0); //将 \n读取出去
                else
                    c = '\n'; // 只要结尾不是\n将c赋值\n
            }
            buf[i] = c; //读取数据。
            i++;
        }
        else
            c = '\n';
    }
    buf[i] = '\0'; //结束
    return i;
}

/**********************************************************************/
/* Inform the client that the requested web method has not been
 * implemented.
 * Parameter: the client socket */
/**********************************************************************/
// 此时无法进行响应
void unimplemented(int client) {
    char buf[1024];

    sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");
    // 以固定格式输出函数
    send(client, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><HEAD><TITLE>Method Not Implemented\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</TITLE></HEAD>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>HTTP request method not supported.\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Give a client a 404 not found status message. */
/**********************************************************************/
void not_found(int client) {
    char buf[1024];

    sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>The server could not fulfill\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "your request because the resource specified\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "is unavailable or nonexistent.\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Inform the client that a request it has made has a problem.
 * Parameters: client socket */
/**********************************************************************/
void bad_request(int client) {
    char buf[1024];

    sprintf(buf, "HTTP/1.0 400 BAD REQUEST\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "Content-type: text/html\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "<P>Your browser sent a bad request, ");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "such as a POST without a Content-Length.\r\n");
    send(client, buf, sizeof(buf), 0);
}

/**********************************************************************/
/* Inform the client that a CGI script could not be executed.
 * Parameter: the client socket descriptor. */
/**********************************************************************/
void cannot_execute(int client)
{
    char buf[1024];

    sprintf(buf, "HTTP/1.0 500 Internal Server Error\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<P>Error prohibited CGI execution.\r\n");
    send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Send a regular file to the client.  Use headers, and report
 * errors to client if they occur.
 * Parameters: a pointer to a file structure produced from the socket
 *              file descriptor
 *             the name of the file to serve */
/**********************************************************************/
void serve_file(int client, const char *filename) {
    FILE * resource = NULL;
    int numchars = 1;
    char buf[1024];

    buf[0] = 'A'; buf[1] = '\0';
    while ((numchars > 0) && strcmp("\n", buf))
        // read & discard headers
        numchars = get_line(client, buf, sizeof(buf));
    resource = fopen(filename, "r");
    if (resource == NULL)
        not_found(client);
    else {
        headers(client, filename);
        cat(client, resource);
    }
    fclose(resource);
}

/**********************************************************************/
/* Return the informational HTTP headers about a file. */
/* Parameters: the socket to print the headers on
 *             the name of the file */
/**********************************************************************/
void headers(int client, const char *filename)
{
    char buf[1024];
    (void)filename;  /* could use filename to determine file type */

    strcpy(buf, "HTTP/1.0 200 OK\r\n");
    send(client, buf, strlen(buf), 0);
    strcpy(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    strcpy(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Put the entire contents of a file out on a socket.  This function
 * is named after the UNIX "cat" command, because it might have been
 * easier just to do something like pipe, fork, and exec("cat").
 * Parameters: the client socket descriptor
 *             FILE pointer for the file to cat */
/**********************************************************************/
void cat(int client, FILE *resource) {
    char buf[1024];
    fgets(buf, sizeof(buf), resource);
    //  char *fgets(char *str, int n, FILE *stream) 从指定的流 stream 读取一行，并把它存储在 str 所指向的字符串内。
    //  当读取 (n-1) 个字符时，或者读取到换行符时，或者到达文件末尾时，它会停止，具体视情况而定。
    while (!feof(resource)) {
        //测试指定流的文件结束标识符，当设置了与流关联的文件结束标识符时，该函数返回一个非零值，否则返回零。
        send(client, buf, sizeof(buf), 0);
        fgets(buf, sizeof(buf), resource);
    }
}

/**********************************************************************/
/* Execute a CGI script.  Will need to set environment variables as
 * appropriate.
 * Parameters: client socket descriptor
 *             path to the CGI script */
/**********************************************************************/

void execute_cgi(int client, const char *path, const char *method, const char *query_string) {
    // 缓冲区
    char buf[1024];
    // 两根管道
    int cgi_output[2]; //[0] out [1] in
    int cgi_input[2];
    // 进程pid和状态
    pid_t pid;
    int status;
    int i;
    char c;
    // 读取字符数
    int numchars = 1;
    //http的content_length
    int content_length = -1;
    //默认字符
    buf[0] = 'A'; buf[1] = '\0';
    if (strcasecmp(method, "GET") == 0)
        //读取数据，把整个header都读掉，以为Get写死了直接读取index.html，没有必要分析余下的http信息了
        while ((numchars > 0) && strcmp("\n", buf)) // read & discard headers
            numchars = get_line(client, buf, sizeof(buf));
    else {
        //post
        numchars = get_line(client, buf, sizeof(buf));
        while ((numchars > 0) && strcmp("\n", buf)) {
            buf[15] = '\0';
            if (strcasecmp(buf, "Content_Length:") == 0)
                content_length = atoi(&buf[16]);
            numchars = get_line(client, buf, sizeof(buf));
        }
        if (content_length == -1) {
            bad_request(client);
            return;
        }
    }

    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    send(client, buf, sizeof(buf), 0);
    // 建立output管道
    if (pipe(cgi_output) < 0) { //成功返回0，失败返回-1
        cannot_execute(client);
        return;
    }
    // 建立input管道
    if (pipe(cgi_input) < 0) {
        cannot_execute(client);
        return;
    }
    // fork后管道都复制了一份，都是一样的
    //       子进程关闭2个无用的端口，避免浪费
    //       ×<------------------------->1    output
    //       0<-------------------------->×   input
    //       父进程关闭2个无用的端口，避免浪费
    //       0<-------------------------->×   output
    //       ×<------------------------->1    input
    //       此时父子进程已经可以通信
    if ( (pid = fork()) < 0 ) {
        cannot_execute(client);
        return;
    }
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    send(client, buf, strlen(buf), 0);
    //父进程用于收数据以及发送子进程处理的回复数据
    if (pid == 0) {
        // child: CGI script
        char meth_env[255];
        char query_env[255];
        char length_env[255];
        // //子进程输出重定向到output管道的1端
        dup2(cgi_output[1], 1); // 重新定向
        //子进程输入重定向到input管道的0端
        dup2(cgi_input[0], 0);
        close(cgi_output[0]); //关闭输出的出口
        close(cgi_input[1]); //关闭输入的入口
        //CGI环境变量
        sprintf(meth_env, "REQUEST_METHOD=%s", method);
        putenv(meth_env); //增加与改变环境变量
        if (strcasecmp(method, "GET") == 0) {
            sprintf(query_env, "QUERY_STRING=%s", query_string);
            putenv(query_env);
        }
        else {//post
            sprintf(length_env, "CONTENT_LENGTH=%d", content_length);
            putenv(length_env);
        }
        execl(path, path, NULL);//pathname: 要执行的文件的路径（推荐使用绝对路径）
        //      pathname: 要执行的文件的路径（推荐使用绝对路径）
        //      第二个参数是一个字符串，是第一个参数中指定的可执行文件的参数（可以是多个参数）
        //          第一个参数是程序名称（没啥用）
        //          第二个开始才是程序的参数
        //          最后一个参数需要null结束（用于告知execl参数列表的结束——哨兵）
        // int m = execl(path, path, NULL);
        // 如果path有问题，例如将html网页改成可执行的，但是执行后m为-1
        // 退出子进程，管道被破坏，但是父进程还在往里面写东西，触发Program received signal SIGPIPE, Broken pipe.
        exit(0);
    }
    else { //parent
        close(cgi_output[1]); //关闭输出的入口
        close(cgi_input[0]); //关闭输入的出口
        if (strcasecmp(method, "POST") == 0) {
            for (i = 0; i < content_length; ++i) {
                recv(client, &c, 1, 0);
                write(cgi_input[1], &c, 1);
            }
            while (read(cgi_output[0], &c, 1) > 0)
                send(client, &c, 1, 0);
        }
        close(cgi_output[0]);
        close(cgi_input[1]);
        waitpid(pid, &status, 0);
        //pid等待终止的目标子进程ID，如传递-1，则与wait函数相同，等待任意子进程终止。
    }
}

/**********************************************************************/
/* Print out an error message with perror() (for system errors; based
 * on value of errno, which indicates system call errors) and exit the
 * program indicating an error. */
/**********************************************************************/
void error_die(const char *sc)
{
    perror(sc); //把一个描述性错误消息输出到标准错误 stderr。
    exit(1);
}

/**********************************************************************/
/* This function starts the process of listening for web connections
 * on a specified port.  If the port is 0, then dynamically allocate a
 * port and modify the original port variable to reflect the actual
 * port.
 * Parameters: pointer to variable containing the port to connect on
 * Returns: the socket */
/**********************************************************************/
int startup(u_short *port) {
    int httpd = 0;
    struct sockaddr_in name;

    httpd = socket(PF_INET, SOCK_STREAM, 0);
    if (httpd == -1)
        error_die("socket");
    memset(&name, 0, sizeof(name));
    name.sin_family = AF_INET;
    name.sin_port = htons(*port);
    name.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(httpd, (struct sockaddr*)&name, sizeof(name)) < 0)
        error_die("bind");
    if (*port == 0) { // 动态分配端口
        //if dynamically allocating a port
        socklen_t namelen = sizeof(name);
        if (getsockname(httpd, (struct sockaddr*)&name, &namelen) == -1)
            // 在以端口号为0调用bind（告知内核去选择本地临时端口号）后，getsockname用于返回由内核赋予的本地端口号。
            // getsockname 用于获取与套接字相关联的本地协议地址
            error_die("getsockname");
        *port = ntohs(name.sin_port);
    }
    if (listen(httpd, 5) < 0)
        error_die("listen");
    return (httpd);
}

int main(void) {
    int server_sock = -1;
    u_short port = 0;
    int client_sock = -1;
    struct sockaddr_in client_name;
    socklen_t client_name_len = sizeof(client_name);

    server_sock = startup(&port);
    printf("httpd running on port %d\n", port);
    while(1) {
        client_sock = accept(server_sock, (struct sockaddr*)&client_name, &client_name_len);
        if (client_sock == -1)
            error_die("accept");
        accept_request(client_sock);
    }
    close(server_sock);
    return 0;
}
