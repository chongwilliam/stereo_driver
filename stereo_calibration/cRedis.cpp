#include "cRedis.h"

#include <iostream>
#include <stdexcept>
#include <algorithm>

using namespace std;

cRedis::cRedis(const std::string& a_ip/* = "127.0.0.1" */)
{
    //    HiredisServerInfo server;
    // Server config
    // TODO: this can be loaded from a autogen file later from the server
    //    m_redis_hostname = "127.0.0.1";
    m_redis_hostname = a_ip;
    m_redis_port = 6379;
    m_redis_timeout = { 1, 500000 }; // 1.5 seconds

    // set up signal handler
    //    signal(SIGABRT, &sighandler);
    //    signal(SIGTERM, &sighandler);
    //    signal(SIGINT, &sighandler);

    //    auto redis_db_ = new CDatabaseRedisClient();
    //    redis_db_->serverIs(server);

    // ----------------------------------------------
    // Creaate Redis Context
    // ----------------------------------------------

    //    std::cout << "m_redis_context = " << m_redis_context << std::endl;
    std::cout << "m_redis_hostname = " << m_redis_hostname << std::endl;
    std::cout << "m_redis_port = " << m_redis_port << std::endl;

    m_redis_context = NULL;

    // delete existing connection
    if (NULL != m_redis_context || NULL == m_redis_hostname.c_str())
    {
        redisFree(m_redis_context);
    }

    if (NULL == m_redis_hostname.c_str())
    {
        // nothing to do
        return;
    }
    // set new server info
    //    server_info_ = server;
    // connect to new server
    auto tmp_context = redisConnectWithTimeout(m_redis_hostname.c_str(), m_redis_port, m_redis_timeout);

    if (NULL == tmp_context)
    {
        throw(runtime_error("Could not allocate redis context."));
    }
    if (tmp_context->err)
    {
        std::string err = std::string("Could not connect to redis server : ") + std::string(tmp_context->errstr);
        redisFree(tmp_context);
        throw(runtime_error(err.c_str()));
    }
    // set context, ping server
    m_redis_context = tmp_context;


    m_ctr_pipe = 0;

}






void cRedis::setRedis(const std::string &a_key, long double a_value)
{
    std::string value = std::to_string( a_value );
    redisReply* reply = (redisReply *)redisCommand(m_redis_context, "SET %s %s", a_key.c_str(), value.c_str() );
    freeReplyObject( (void*) reply);
}



void cRedis::setRedisPipeline(const std::string &a_key, long double a_value)
{
    std::string value = std::to_string( a_value );
    redisAppendCommand(m_redis_context, "SET %s %s", a_key.c_str(), value.c_str()  );

    m_ctr_pipe++;

    // redisReply* reply = (redisReply *)redisCommand(m_redis_context, "SET %s %s", a_key.c_str(), value.c_str() );
    // freeReplyObject( (void*) reply);
}


void cRedis::freePipeline()
{
    //        redisReply* reply = (redisReply *) redisCommand(m_redis_context,"SUBSCRIBE foo");
    //        freeReplyObject(reply);

    void* reply;


    for (int i=0; i<m_ctr_pipe; ++i)
    {
        redisGetReply(m_redis_context, &reply);
        freeReplyObject(reply);
    }

    m_ctr_pipe = 0;
}



void cRedis::setRedis(const std::string &a_key, const std::string &a_value)
{
    redisReply* reply = (redisReply *)redisCommand(m_redis_context, "SET %s %s", a_key.c_str(), a_value.c_str() );
    freeReplyObject( (void*) reply);
}



void cRedis::getRedisPipeline(const std::vector<string> a_vec_keys, std::vector<string>& a_vec_values)
{

    int size = a_vec_keys.size();
    a_vec_values.resize( size );



    for (int i=0; i<size; ++i)
    {
        redisAppendCommand(m_redis_context, "GET %s", a_vec_keys.at(i).c_str()  );
    }


    void* reply;

    for (int i=0; i<size; ++i)
    {
        redisGetReply(m_redis_context, &reply);
        redisReply* reply_red = (redisReply *) reply;

        std::string value = reply_red->str;
        a_vec_values.at(i) = value;
        freeReplyObject((void*) reply);
    }


}

std::string cRedis::getValueByKeyInVectors(const std::vector<std::string> &a_vec_keys, const std::vector<std::string> &a_vec_values, const std::string& a_key)
{
    auto it = std::find(a_vec_keys.begin(), a_vec_keys.end(), a_key);
    if (it == a_vec_keys.end())
    {
        std::cout << "cRedis::getIndexOfVectorElement: key not in vector!" << std::endl;
        return "NO";
    } else
    {
        auto index = std::distance(a_vec_keys.begin(), it);
        return a_vec_values[index];
    }
}

double cRedis::getRedisDouble(const std::string &a_key)
{
    redisReply* reply = (redisReply *)redisCommand(m_redis_context, "GET %s", a_key.c_str() );
    std::string value = reply->str;
    freeReplyObject((void*) reply);

    //    std::cout << "key = " << a_key << std::endl;
    //    std::cout << "value = " << value << std::endl;
    return stod(value);
}

std::string cRedis::getRedisString(const std::string &a_key)
{
    redisReply* reply = (redisReply *)redisCommand(m_redis_context, "GET %s", a_key.c_str() );
    std::string value = reply->str;
    freeReplyObject((void*) reply);

    return value;
}




string cRedis::vecToStr(const vector<string> &vec, const char delimiter) {
    string str;
    for (size_t i = 0; i < vec.size() - 1; i++)
        str += vec[i] + delimiter;
    str += vec[vec.size() - 1];
    return str;
}

vector<string> cRedis::strToVec(const string &s, const char delimiter) {
    vector<string> vec;
    size_t last = 0;
    size_t next = 0;
    while ((next = s.find(delimiter, last)) != string::npos) {
        vec.push_back(s.substr(last, next - last));
        last = next + 1;
    }
    vec.push_back(s.substr(last));
    return vec;
}



void cRedis::setJpegBinary(const string &a_key, std::vector<unsigned char> &buffer)
{
    // http://stackoverflow.com/questions/42800878/storing-vector-of-chars-in-redis-containing-nul
    redisReply* reply = static_cast<redisReply *>( redisCommand(m_redis_context, "SET %s %b", a_key.c_str(), buffer.data(), buffer.size() ) );
    freeReplyObject( reply );
}

void cRedis::setJpegBinaryStereo(const std::string& a_key_L, std::vector<unsigned char>& buffer_L, const std::string& a_key_R, std::vector<unsigned char>& buffer_R)
{
    redisAppendCommand(m_redis_context, "SET %s %b", a_key_L.c_str(), buffer_L.data(), buffer_L.size() );
    redisAppendCommand(m_redis_context, "SET %s %b", a_key_R.c_str(), buffer_R.data(), buffer_R.size() );

    void* reply;
    for (int i=0; i<2; ++i)
    {
        redisGetReply(m_redis_context, &reply);
        freeReplyObject(reply);
    }

}


std::vector<unsigned char> cRedis::getJpegBinary(const string &a_key)
{
#if 0

    redisReply* reply = (redisReply *) redisCommand(m_redis_context, "GET %s", a_key.c_str() );
    if ( !reply )
        std::cout << "cRedis::getJpegBinary ERROR 0" << std::endl;
    //        return REDIS_ERR;
    if ( reply->type != REDIS_REPLY_STRING ) {
        std::cout << "cRedis::getJpegBinary ERROR 1" << std::endl;
        //        printf("ERROR: %s", reply->str);
    } else {

        // Here, it is safer to make a copy to be sure memory is properly aligned
        unsigned char *val = (unsigned char *) malloc( reply->len );
        memcpy( val, reply->str, reply->len);
        //        for (int i=0; i<reply->len/sizeof(unsigned char); ++i )
        //        {
        //            printf("%d\n",val[i]);
        //            //            std::cout << (unsigned char) val[i] << std::endl;
        //        }

        //        std::vector<unsigned char> jped_returned(std::begin(val), std::end(val));
        std::vector<unsigned char> jped_returned(val, val + reply->len);

        return jped_returned;

//        int num_returned = jped_returned.size();
//        for (int i=0; i<num_returned; ++i )
//        {
//            std::cout << "jped_returned[i] = " << jped_returned[i] << std::endl;
//        }

//        std::cout << "jped_returned.size() = " << jped_returned.size() << std::endl;
//        std::cout << "reply->len = " << reply->len << std::endl;

        free( val );
    }
    freeReplyObject(reply);


#endif

//    redisReply* reply = (redisReply *) redisCommand(m_redis_context, "GET %s", a_key.c_str() );
//    unsigned char *val = (unsigned char *) malloc( reply->len );
//    memcpy( val, reply->str, reply->len);
//    std::vector<unsigned char> jped_returned(val, val + reply->len);
//    free( val );
//    freeReplyObject(reply);

//    return jped_returned;


    redisReply* reply = static_cast<redisReply *>(redisCommand(m_redis_context, "GET %s", a_key.c_str() ) );
    std::vector<unsigned char> read_vector( reply->str, reply->str+reply->len );
    freeReplyObject(reply);
    return read_vector;

}
