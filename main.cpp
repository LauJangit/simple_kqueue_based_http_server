//
//  main.cpp
//  simple_http_server
//
//  Created by Jangit Lau on 2018/1/6.
//  Copyright © 2018年 jangit.cn. All rights reserved.
//
// 网页文件请放置在根目录下


#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <iostream>
#include <assert.h>
#include <netinet/in.h>
#include <unistd.h>
#include <unordered_map>
#include <fstream>
#include <string>
#include <sstream>
#include <iostream>


class ret_header{
    static std::string get_ext_name(std::string filename){
        std::string ext_name="";
        int idx_start=(int)filename.find_last_of(".");
        if(idx_start<0){
            ext_name="html";
        }else{
            int idx_end=(int)filename.find_last_of("?")-(idx_start+1);
            if(idx_end>0){
                ext_name=filename.substr(idx_start+1,idx_end);
            }else{
                ext_name=filename.substr(idx_start+1);
            }
        }
        return ext_name;
    }
    
    static std::string itos(int number){
        std::stringstream ss;
        ss<<number;
        return ss.str();
    }
    
    static std::string get_return_code(int _return_code){
        std::string ret_str="";
        switch (_return_code) {
            case 200:
            ret_str="200 OK";
            break;
            case 400:
            ret_str="400 Bad Request";
            break;
            case 403:
            ret_str="403 Forbidden";
            break;
            case 404:
            ret_str="404 Not Found";
            break;
            case 405:
            ret_str="405 Method Not Allowed";
            break;
        }
        return ret_str;
    }
    public:
    static std::string make_header(int _ret_code,std::string _ret_file_path,int _content_length){
        std::string ret_header="";
        ret_header+="HTTP/1.1 "+get_return_code(_ret_code)+"\r\n";
        ret_header+="Connection: keep-alive\r\n";
        ret_header+="Server: jangit`s kqueue server\r\n";
        if(_ret_file_path!=""){
            ret_header+="Content-Type: text/"+get_ext_name(_ret_file_path)+";charset=utf-8\r\n";
            ret_header+="Content-Length:"+itos(_content_length)+"\r\n";
        }
        ret_header+="\r\n";
        return ret_header;
    }
    
};

class Packet{
    int file_descriptor=0;
    std::string header="",path="",ret_packet_content="";
    
    int end_status=0;
    int http_method=0;
    int path_status=0;
    
    std::string uri_to_filepath(std::string _path){
        int idx_end=(int)_path.find_last_of("?");
        if(idx_end<0){
            return "."+_path;
        }else{
            return "."+_path.substr(0,idx_end);
        }
    }
    
    std::string get_response(std::string path){
        if(path=="/"){
            path="/index.html";
        }
        std::string real_path=uri_to_filepath(path);
        std::cout<<"real_path:"<<real_path<<std::endl;
        
        std::string ret_header="",ret_body="";
        
        std::ifstream target_file(real_path,std::ios::binary);
        if(!target_file.is_open()){
            target_file.close();
            if(access(real_path.c_str(),0)==-1){
                ret_header=ret_header::make_header(404,"",0);
            }else if(access(real_path.c_str(),4)==-1){
                ret_header=ret_header::make_header(403,"",0);
            }else{
                ret_header=ret_header::make_header(403,"",0);
            }
        }else{
            //读取文件(先获取文件长度，再整个读取文件)
            target_file.seekg(0, std::ios::end);
            int file_length = (int)target_file.tellg();
            target_file.seekg(0, std::ios::beg);
            char *file_buffer = new char[file_length];
            target_file.read(file_buffer, file_length);
            ret_body.append(file_buffer,file_length);
            delete[] file_buffer;
            //设置头部
            ret_header=ret_header::make_header(200, path, (int)ret_body.length());
        }
        target_file.close();
        return ret_header+ret_body;
    }
    
    bool read_header(char chr){
        header+=chr;
        
        if(http_method==0&&header.length()==1){
            if(chr=='G'||chr=='g'){
                http_method=1;
            }else{
                http_method=-1;
            }
        }else if(http_method==1){
            if(chr=='E'||chr=='e'){
                http_method=2;
            }else{
                http_method=-1;
            }
        }else if(http_method==2){
            if(chr=='T'||chr=='t'){
                http_method=10;
            }else{
                http_method=-1;
            }
        }
        
        if(chr=='/'&&path_status==0){
            path_status=1;
            path+=chr;
        }else if(path_status==1){
            if(chr!=' '){
                path+=chr;
            }else{
                path_status=10;
            }
        }
        
        if(chr=='\r'&&end_status==0){
            end_status=1;
        }else if(chr=='\n'&&end_status==1){
            end_status=2;
        }else if(chr=='\r'&&end_status==2){
            end_status=3;
        }else if(chr=='\n'&&end_status==3){
            end_status=-1;
            std::cout<<"收到的数据:"<<std::endl<<header<<std::endl;
            if(http_method==10){
                std::cout<<"GET方法"<<std::endl;
                if(path_status==10){
                    std::cout<<"路径:"<<path<<std::endl;
                    ret_packet_content=get_response(path);
                }else{
                    std::cout<<"未知路径，状态"<<path_status<<std::endl;
                    ret_packet_content=ret_header::make_header(400, "", 0);
                }
            }else{
                std::cout<<"未知方法，状态"<<http_method<<std::endl;
                ret_packet_content=ret_header::make_header(405, "", 0);
            }
            return false;
        }else{
            end_status=0;
        }
        return true;
    }
    public:
    Packet(int _file_descriptor,struct sockaddr_in& client_addr){
        file_descriptor=_file_descriptor;
        char remote[sizeof(client_addr)];
        std::cout<<"新客户端接入，文件描述符:"<<_file_descriptor
        <<" IP地址:"<<inet_ntop(AF_INET, &client_addr.sin_addr, remote, INET_ADDRSTRLEN)
        <<" 来源端口:"<<ntohs(client_addr.sin_port)<<std::endl;
    }
    
    std::string get_ret_http_content(){
        return ret_packet_content;
    }
    
    bool insert_chr(char chr){
        return read_header(chr);
    }
};

class simple_http_server{
    
    private:
    std::string ip;              // IP地址
    int port;               // 绑定的端口
    int back_log;           // tcp中等待accept的数量
    int max_event_count;    // 一个kqueue中最大的文件描述符数量
    bool on_loop;      // 是否进行循环
    std::unordered_map<int, Packet*>* map=NULL;  //处理的包
    private:
    //停止循环
    void make_loop_stop(int signal){
        simple_http_server::on_loop=false;
    }
    
    //构建Socket
    int construct_socket(std::string _ip,int _port,int _back_log){
        // 创建socket文件描述符
        int socket_file_descriptor=socket(PF_INET,SOCK_STREAM,IPPROTO_TCP);
        if(socket_file_descriptor<0){
            std::cout<<"获取Socket文件描述符失败"<<std::endl;
            return -1;
        }else{
            std::cout<<"socket_file_descriptor:"<<socket_file_descriptor<<std::endl;
        }
        
        // 构建地址
        struct sockaddr_in address;
        address.sin_family=AF_INET;
        inet_pton(AF_INET, _ip.c_str(), &(address.sin_addr));// 转换字符串形式IP到二进制形式
        address.sin_port=htons(_port);// 转换端口到对应格式
        
        // 绑定端口
        int bind_retcode=::bind(socket_file_descriptor,(const struct sockaddr*)&address, sizeof(address));
        if(socket_file_descriptor<0){
            std::cout<<"绑定端口失败"<<std::endl;
            return -1;
        }else{
            std::cout<<"bind_retcode:"<<bind_retcode<<std::endl;
        }
        
        // 监听端口
        int listen_retcode=listen(socket_file_descriptor, _back_log);
        if(socket_file_descriptor<0){
            std::cout<<"监听端口失败"<<std::endl;
            return -1;
        }else{
            std::cout<<"listen_retcode:"<<listen_retcode<<std::endl;
        }
        return socket_file_descriptor;
    }
    
    // 初始化kevent
    int init_kqueue(){
        // 获取kqueue标识符
        int kqueue_indicator=kqueue();
        if(kqueue_indicator<0){
            std::cout<<"获取kqueue标识符失败"<<std::endl;
            return -1;
        }else{
            std::cout<<"kqueue_indicator:"<<kqueue_indicator<<std::endl;
        }
        return kqueue_indicator;
    }
    
    // 注册kevent事件
    bool kevent_register(int _event_file_descriptor,int _kqueue_indicator){
        struct kevent change;// 创建一个新的kevent
        EV_SET(&change, _event_file_descriptor, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, 0);// 将新的事件放置于kevent中
        int ret = kevent(_kqueue_indicator, &change, 1, NULL, 0, NULL);// 注册kevent
        if(ret<0){
            std::cout<<"注册kevent事件失败"<<std::endl;
            return false;
        }else{
            std::cout<<"kevent_ret_code:"<<ret<<std::endl;
        }
        return true;
    }
    
    //接受新的请求
    int on_accept(int _socket_file_descriptor,int _kqueue_indicator){
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int new_file_descriptor = accept(_socket_file_descriptor, (struct sockaddr *) &client_addr, &client_addr_len);
        
        if (kevent_register(new_file_descriptor,_kqueue_indicator)){
            (*map)[new_file_descriptor]=new Packet(new_file_descriptor,client_addr);
            std::cout<<"new_file_descriptor:"<<new_file_descriptor<<std::endl;
            return new_file_descriptor;
        }else{
            return -1;
        }
    }
    
    //处理已经被接受的客户端
    void on_process(int buffer_length,int client_file_descriptor){
        char recvChr[buffer_length];
        int recv_ret=(int)recv(client_file_descriptor,recvChr,buffer_length,0);
        if(recv_ret>=0){
            Packet* packet=(*map).at(client_file_descriptor);
            for(int i=0;i<buffer_length;i++){
                bool ret=packet->insert_chr(recvChr[i]);
                if(!ret){
                    std::string ret_str=packet->get_ret_http_content();
                    send(client_file_descriptor,ret_str.c_str(),ret_str.length(),0);
                    map->erase(client_file_descriptor);
                    close(client_file_descriptor);
                    delete packet;
                    break;
                }
            }
        }
    }
    
    //处理kevent
    void handler(int _socket_file_descriptor,int _max_event_count,int _kqueue_indicator){
        struct kevent events[_max_event_count];
        int event_counter=0;
        while (on_loop) {
            event_counter=kevent(_kqueue_indicator, NULL,0 , events, _max_event_count, NULL);
            for(int i=0;i<event_counter;i++){
                struct kevent event = events[i];
                int event_data=(int)event.data;
                int client_file_descriptor = (int) event.ident;
                if(client_file_descriptor==_socket_file_descriptor){
                    on_accept(_socket_file_descriptor,_kqueue_indicator);
                }
                if((*map).find(client_file_descriptor)!=(*map).end()){
                    on_process(event_data,client_file_descriptor);
                }
            }
        }
    }
    void close(int _socket_file_descriptor){
        ::close(_socket_file_descriptor);
    }
    public:
    
    // 构造该类
    simple_http_server(std::string _ip,int _port,int _back_log,int _max_event_count){
        ip=_ip;
        port=_port;
        back_log=_back_log;
        max_event_count=_max_event_count;
        on_loop=true;
        map=new std::unordered_map<int, Packet*>();
    }
    
    //关闭该类
    ~simple_http_server(){
        std::cout<<"exit"<<std::endl;
        delete map;
    }
    
    // 开始运行
    void start(){
        int socket_file_descriptor=construct_socket(ip,port,back_log);
        int kqueue_indicator=init_kqueue();
        if(!kevent_register(socket_file_descriptor,kqueue_indicator)){
            return;
        }
        handler(socket_file_descriptor, max_event_count, kqueue_indicator);
        close(socket_file_descriptor);
    }
    
};


int main(int argc, const char * argv[]) {
    simple_http_server server("0.0.0.0",10000,128,128);
    server.start();
    return 0;
}

