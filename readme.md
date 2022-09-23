# Tinyhttpd

每个函数的作用：

   accept_request:  处理从套接字上监听到的一个 HTTP 请求，在这里可以很大一部分地体现服务器处理请求流程。

   bad_request: 返回给客户端这是个错误请求，HTTP 状态吗 400 BAD REQUEST.

   cat: 读取服务器上某个文件写到 socket 套接字。

   cannot_execute: 主要处理发生在执行 cgi 程序时出现的错误。

   error_die: 把错误信息写到 perror 并退出。

   execute_cgi: 运行 cgi 程序的处理，也是个主要函数。

   get_line: 读取套接字的一行，把回车换行等情况都统一为换行符结束。

   headers: 把 HTTP 响应的头部写到套接字。

   not_found: 主要处理找不到请求的文件时的情况。

   sever_file: 调用 cat 把服务器文件返回给浏览器。

   startup: 初始化 httpd 服务，包括建立套接字，绑定端口，进行监听等。

   unimplemented: 返回给浏览器表明收到的 HTTP 请求所用的 method 不被支持。

## startup

```c
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
```

## accept_request

取出 HTTP 请求中的 method (GET 或 POST) 和 url,。对于 GET 方法，如果有携带参数，则 query_string 指针指向 url 中 ？ 后面的 GET 参数。

```c
# HTTP请求（Request)
'''
当用户通过浏览器访问某个网站时，
浏览器会向网站服务器发送请求，这个请求就叫做HTTP请求。
请求包含的内容主要有:
请求方法（Request Method);
请求网址(Request URL);
请求头（Request Headers);
请求体(Request Body)。
'''

# 下面来看一下浏览器向百度的网站服务器发送了哪些信息。
#1.请求方法（Request Method)
'''
HTTP协议定义了许多与服务器交互的方法，最常用的是GET和POST方法。
如果浏览器向服务器发送一个GET请求，
则请求的参数信息会直接包含在URL中。
例如在百度搜索栏中输入scrapy，单击"百度一下“按钮，就形成了一个GET请求。
搜索结果页面的URL变为https://www.baidu.com/s?wd=scrapy,
URL中问号（？）后面的wd=scrapy就是请求的参数，表示要搜索的关键字。

POST请求主要用于表单的提交。
表单中输入的卡号、密码等隐私信息通过POST请求方式提交后，
数据不会暴露在URL中，而是保存于请求体中，避免了信息的泄露。
'''

# 2.请求网址（Request URL)
'''
另外，还有一个选项Remote Address: 14.215.177.38:443，这是百度服务器的IP地址。
也可以使用IP地址来访问百度。
'''

# 3.请求头（Request Headers)
'''
请求头的内容在Headers选项卡中的Request Headers目录下，
如下图所示。请求头中包含了许多有关客户端环境和请求正文的信息，
比较重要的信息有Cookie和User-Agent等。
  
Accept:浏览器端可以接收的媒体类型。
text/html代表浏览器可以接收服务器发送的文档类型为text/html,也就是我们常说的HTML文档。
Accept-Encoding:浏览器接受的编码方式。
Accept-Language:浏览器所接受的语言种类。
Connection:表示是否需要持久连接。keep-alive表示浏览器与网站服务器保持连接；close表示一个请求结束后，浏览器和网站服务器就会断开，下次请求时需重新连接。
Cookie:有时也用复数形式Cookies，指网站为了提高用户身份、进行会话跟踪而存储在本地的数据（通常经过加密），由网站服务器创建。
例如当我们登录后，访问该网站的其他页面时，发现都是处于登录状态，这是Cookie在发挥作用。
因为浏览器每次在请求该站点的页面时，都会在请求头上加上保存有用户名和密码等信息的Cookie并将其发送给服务器，
服务器识别出该用户后，就将页面发送给服务器。
在爬虫中，有时需要爬取登录后才能访问的页面，通过对Cookie进行设置，就可以成功访问登录后的页面了。

Host:指定被请求资源的Internet主机和端口号，通常从URL中提取。

User-Agent:告诉网站服务器，客户端使用的操作系统、浏览器的名称和版本、CPU版本，以及浏览器渲染引擎、浏览器语言等。
在爬虫中，设置此项可以将爬虫伪装成浏览器。
'''
  
# 4.请求体（Request Body)
'''
请求体中保存的内容一般是POST请求发送的表单数据。
对于GET请求，请求体为空。
'''
```

并非所有出现在请求中的 HTTP 首部都属于请求头，例如在 [`POST`](https://developer.mozilla.org/zh-CN/docs/Web/HTTP/Methods/POST) 请求中经常出现的 [`Content-Length`](https://developer.mozilla.org/zh-CN/docs/Web/HTTP/Headers/Content-Length) 实际上是一个代表请求主体大小的 [entity header](https://developer.mozilla.org/zh-CN/docs/Glossary/Entity_header)，虽然你也可以把它叫做请求头。

下面是一个 HTTP 请求的请求头：

```
GET / HTTP/1.1
Host: 192.168.0.23:47310
Connection: keep-alive
Upgrade-Insecure-Requests: 1
User-Agent: Mozilla/5.0 (Windows NT 6.1; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/55.0.2883.87 Safari/537.36
Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*; q = 0.8
Accept - Encoding: gzip, deflate, sdch
Accept - Language : zh - CN, zh; q = 0.8
Cookie: __guid = 179317988.1576506943281708800.1510107225903.8862; monitor_count = 5
```

严格来说在这个例子中的 [`Content-Length`](https://developer.mozilla.org/zh-CN/docs/Web/HTTP/Headers/Content-Length) 不是一个请求头

```
POST /myform.html HTTP/1.1
Host: developer.mozilla.org
User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10.9; rv:50.0) Gecko/20100101 Firefox/50.0
Content-Length: 128
```

```c
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
    i = 0; j = 0;
    while (!ISspace(buf[j]) && (i < sizeof(method) - 1)) { //检查非空
        method[i] = buf[j];
        i++;j++;
    }
    if (strcasecmp(method, "GET") && strcasecmp(method, "POST")) {
        // strcasecmp(判断函数s1与s2大小)，s1=s2返回0；此时满足都不等于
        unimplemented(client);
    }
    if (strcasecmp(method, "POST") == 0)
        cgi = 0;
    i = 0;
    // 判断是否空白字符并将空白字符间隔出去
    while (ISspace(buf[j]) && j < sizeof(buf))
        j++;
    // 获取url
    while (!ISspace(buf[j]) && (i < sizeof(url) - 1) && (j < sizeof(buf))) {
        url[i] = buf[j];
        i++; j++;
    }
    url[i] = '\0';
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

    sprintf(path, "htdocs%s", url);
    if (path[strlen(path) - 1] == '/')
        strcat(path, "index.html"); //将函数拼接起来
    if (stat(path, &st) == -1) { //获取文件信息，没有获取到
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

```



## get_line

读取套接字的一行，把回车换行等情况统一换为换行符结束

```c
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
                    c = '\n'; // 只要结尾\r之后不是\n将c赋值\n
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
```

## execute_cgi

```c
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
```

