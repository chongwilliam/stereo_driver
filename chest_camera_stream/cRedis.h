
// https://github.com/redis/hiredis


#ifndef CREDIS_H
#define CREDIS_H

#include <hiredis.h>
#include <string>
#include <sstream>
#include <vector>

class cRedis
{
public:
    cRedis(const std::string& a_ip = "127.0.0.1");
    double getRedisDouble(const std::string& a_key);
    std::string getRedisString(const std::string& a_key);
    void setRedis(const std::string& a_key, long double a_value);
    void setRedisPipeline(const std::string &a_key, long double a_value);

    void freePipeline();
    void setRedis(const std::string& a_key, const std::string& a_value);

    void getRedisPipeline(const std::vector<std::string> a_vec_keys, std::vector<std::string> &a_vec_values);

    std::string getValueByKeyInVectors(const std::vector<std::string>& a_vec_keys, const std::vector<std::string>& a_vec_values, const std::string& a_key);


    static std::string vecToStr(const std::vector<std::string> &vec, const char delimiter = ' ');
    static std::vector<std::string> strToVec(const std::string &s, const char delimiter = ' ');



    void setJpegBinary(const std::string& a_key, std::vector<unsigned char>& buffer);
    std::vector<unsigned char> getJpegBinary(const std::string& a_key);

    void setJpegBinaryStereo(const std::string& a_key_L, std::vector<unsigned char>& buffer_L, const std::string& a_key_R, std::vector<unsigned char>& buffer_R);



private:
    redisContext* m_redis_context;
    redisReply*   m_redis_reply;

    std::string m_redis_hostname;
    int m_redis_port;
    timeval m_redis_timeout;

    int m_ctr_pipe; // counts the number of setRedisPipeline() function calls


};

#endif // CREDIS_H
