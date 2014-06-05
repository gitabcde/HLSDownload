#include "CHLSPaser.h"
#include "libcurl/curl.h"
#include <vector>
#include <map>
#include <algorithm>
#include "openssl/aes.h"

static int WriteToString(void* src,int size,int count,void* dst)
{
	std::string* str=(std::string*)dst;
	int len=size*count;
	str->append((char*)src,len);	
	return len;
}
CHLSPaser::CHLSPaser()
{
	m_ManifestUrl="";
	m_ManifestContent="";
	m_UpdatedManifestContent="";
	m_Cookie="";
	m_CurrentFragPos=0;
	m_bRelativeFragUrl=false;
	m_BaseUrl="";
	m_currentkey="";
	m_currentiv="";
	m_currentkeyivpos=0;
	m_nextkeyivpos=0;
	m_IsEncrypt=false;
	m_BufferSize=500*1000;
	m_DecryptBuffer=new char[m_BufferSize];
}
CHLSPaser::~CHLSPaser()
{

}
int CHLSPaser::SetManifestUrl(std::string url)
{
	m_ManifestUrl=url;
	m_BaseUrl=m_ManifestUrl.substr(0,m_ManifestUrl.rfind("/")+1);
	DownloadToString(url,m_ManifestContent);
	//如果链接是一个存储了M3U8列表地址的文件,找到一个比特率排名第三的M3U8列表
	if(m_ManifestContent.find(HLS_STREAM_BEGIN_FLAG)!=std::string::npos)
	{
		std::string::size_type urlbegin=0,urlend=0;
		if(m_ManifestContent.find("BANDWIDTH")==std::string::npos)
		{
			urlbegin=m_ManifestContent.find(HLS_STREAM_BEGIN_FLAG);
			urlbegin=m_ManifestContent.find("\n",urlbegin);
			urlend=m_ManifestContent.find("\n",urlbegin);
		}
		else
		{
			std::string::size_type currentbandwidthpos=0;
			int currentbandwidth=0;
			std::vector<int> bws;
			std::map<int,std::string::size_type> bwpos;
			while((currentbandwidthpos=m_ManifestContent.find("BANDWIDTH",currentbandwidthpos))!=std::string::npos)
			{
				currentbandwidthpos=m_ManifestContent.find("=",currentbandwidthpos)+1;
				std::string::size_type end=m_ManifestContent.find(",",currentbandwidthpos);
				std::string strbw=m_ManifestContent.substr(currentbandwidthpos,end-currentbandwidthpos);
				currentbandwidth=atoi(strbw.c_str());
				bws.push_back(currentbandwidth);
				bwpos.insert(std::pair<int,std::string::size_type>(currentbandwidth,currentbandwidthpos));
			}
			std::sort(bws.begin(),bws.end());
			std::string::size_type selectedbandwidthpos=bwpos[bws[bws.size()/2]];
			urlbegin=m_ManifestContent.find("\n",selectedbandwidthpos)+1;
			urlend=m_ManifestContent.find("\n",urlbegin);
		}
		std::string m3u8url=m_ManifestContent.substr(urlbegin,urlend-urlbegin);
		if(m3u8url.substr(0,4)!="http")
			m3u8url=m_BaseUrl+m3u8url;
		m_ManifestUrl=m3u8url;
		m_BaseUrl=m3u8url.substr(0,m3u8url.rfind("/")+1);
		m_ManifestContent.clear();
		DownloadToString(m3u8url,m_ManifestContent);

	}
	m_CurrentFragPos=m_ManifestContent.find(HLS_FRAG_BEGIN_FLAG,m_CurrentFragPos);
	int begin,end;
	//测试frag网址是否是相对地址
	begin=m_ManifestContent.find("\n",m_CurrentFragPos)+1;
	if(m_ManifestContent.substr(begin,4)!="http")
		m_bRelativeFragUrl=true;
	else
		m_bRelativeFragUrl=false;
	//检测frag的持续时间
	begin=m_ManifestContent.find(":",m_CurrentFragPos)+1;
	end=m_ManifestContent.find(",",m_CurrentFragPos);
	m_FragDuration=atoi(m_ManifestContent.substr(begin,end-begin).c_str());
	//检测是否为直播
	if(m_ManifestContent.find(HLS_NOT_LIVE_FLAG)!=std::string::npos)
		m_bIsLive=false;
	else
		m_bIsLive=true;
	if(m_ManifestContent.find(HLS_KEY_FLAG)!=std::string::npos)
	{
		m_IsEncrypt=true;

		begin=m_ManifestContent.find(HLS_KEY_FLAG);

		begin=m_ManifestContent.find(HLS_ENCRYPTMODE_FLAG,begin)+sizeof(HLS_ENCRYPTMODE_FLAG)-1;
		end=m_ManifestContent.find(",",begin);
		m_currentencryptmode=m_ManifestContent.substr(begin,end-begin);

		begin=m_ManifestContent.find(HLS_KEY_URL_FLAG,begin)+sizeof(HLS_KEY_URL_FLAG)-1;
		begin+=1;//跳过开头的引号
		end=m_ManifestContent.find(",",begin);
		end-=1;//跳过结尾的引号
		std::string keyurl=m_ManifestContent.substr(begin,end-begin);
		DownloadToString(keyurl,m_currentkey);

		begin=m_ManifestContent.find(HLS_IV_FLAG,begin)+sizeof(HLS_IV_FLAG)-1;
		end=m_ManifestContent.find("\n",begin);
		m_currentiv=m_ManifestContent.substr(begin,end-begin);
		
		m_nextkeyivpos=m_ManifestContent.find(HLS_KEY_FLAG,end);
	}

	return 0;

}

int CHLSPaser::SetCookie(std::string cookie)
{
	m_Cookie=cookie;
	return 0;
}
int CHLSPaser::GetFragUrl(std::string& fragurl)
{
	if((m_CurrentFragPos=m_ManifestContent.find(HLS_FRAG_BEGIN_FLAG,m_CurrentFragPos))!=std::string::npos)
	{
		std::string::size_type urlbegin,urlend;
		m_CurrentFragPos+=sizeof(HLS_FRAG_BEGIN_FLAG)-1;
		urlbegin=m_ManifestContent.find("\n",m_CurrentFragPos)+1;
		urlend=m_ManifestContent.find("\n",urlbegin);
		if(m_bRelativeFragUrl)
			fragurl=m_BaseUrl+m_ManifestContent.substr(urlbegin,urlend-urlbegin);
		else
			fragurl=m_ManifestContent.substr(urlbegin,urlend-urlbegin);
		m_OldFragUrl=m_ManifestContent.substr(urlbegin,urlend-urlbegin);
		if(m_bIsLive)
		{
			m_UpdatedManifestContent.clear();
			DownloadToString(m_ManifestUrl,m_UpdatedManifestContent);
			m_ManifestContent=m_UpdatedManifestContent;
			if(m_ManifestContent.find(m_OldFragUrl)==std::string::npos)
				m_CurrentFragPos=m_ManifestContent.find(HLS_FRAG_BEGIN_FLAG);
			else
				m_CurrentFragPos=m_ManifestContent.find(HLS_FRAG_BEGIN_FLAG,m_ManifestContent.find(m_OldFragUrl));
			if(m_CurrentFragPos>m_nextkeyivpos)
			{
				std::string::size_type begin,end;
				m_currentkeyivpos=m_nextkeyivpos;
				begin=m_ManifestContent.find(HLS_KEY_FLAG);

				begin=m_ManifestContent.find(HLS_ENCRYPTMODE_FLAG,begin)+sizeof(HLS_ENCRYPTMODE_FLAG)-1;
				end=m_ManifestContent.find(",",begin);
				m_currentencryptmode=m_ManifestContent.substr(begin,end-begin);

				begin=m_ManifestContent.find(HLS_KEY_URL_FLAG,begin)+sizeof(HLS_KEY_URL_FLAG)-1;
				begin+=1;//跳过开头的引号
				end=m_ManifestContent.find(",",begin);
				end-=1;//跳过结尾的引号
				std::string keyurl=m_ManifestContent.substr(begin,end-begin);
				DownloadToString(keyurl,m_currentkey);

				begin=m_ManifestContent.find(HLS_IV_FLAG,begin)+sizeof(HLS_IV_FLAG)-1;
				end=m_ManifestContent.find("\n",begin);
				m_currentiv=m_ManifestContent.substr(begin,end-begin);

				m_nextkeyivpos=m_ManifestContent.find(HLS_KEY_FLAG,end);
			}
		}
		return 0;
	}
	else
	{
		if(!m_bIsLive)
			return -1;
		std::string::size_type urlbegin,urlend;
		m_ManifestContent=m_UpdatedManifestContent;
		if(m_UpdatedManifestContent.find(m_OldFragUrl)==std::string::npos)
			m_CurrentFragPos=m_ManifestContent.find(HLS_FRAG_BEGIN_FLAG);
		else
			m_CurrentFragPos=m_ManifestContent.find(HLS_FRAG_BEGIN_FLAG,m_ManifestContent.find(m_OldFragUrl));
		urlbegin=m_ManifestContent.find("\n",m_CurrentFragPos)+1;
		urlend=m_ManifestContent.find("\n",urlbegin);
		if(m_bRelativeFragUrl)
			fragurl=m_BaseUrl+m_ManifestContent.substr(urlbegin,urlend-urlbegin);
		else
			fragurl=m_ManifestContent.substr(urlbegin,urlend-urlbegin);
		m_OldFragUrl=m_ManifestContent.substr(urlbegin,urlend-urlbegin);
		return 0;
	}
}
int CHLSPaser::DownloadToString(std::string url,std::string& str)
{
	CURL* pCurl=curl_easy_init();
	curl_easy_setopt(pCurl,CURLOPT_URL,url.c_str());
	curl_easy_setopt(pCurl,CURLOPT_WRITEFUNCTION,WriteToString);
	curl_easy_setopt(pCurl,CURLOPT_SSL_VERIFYHOST,0);
	curl_easy_setopt(pCurl,CURLOPT_SSL_VERIFYPEER,0);
	curl_easy_setopt(pCurl,CURLOPT_WRITEDATA,&str);
	curl_easy_setopt(pCurl,CURLOPT_USERAGENT,"Mozilla/5.0 (Windows NT 6.1) AppleWebKit/537.22 (KHTML, like Gecko) Chrome/25.0.1364.152 Safari/537.22");
	CURLcode ret=curl_easy_perform(pCurl);
	if(ret!=CURLE_OK)
		throw std::string("cannot download playlist");
	return 0;
}
int CHLSPaser::DecryptFragData(std::string& fragstr)
{
	AES_KEY aeskey;
	std::string::size_type len=fragstr.size();
	if(len>m_BufferSize)
	{
		delete m_DecryptBuffer;
		m_DecryptBuffer=new char[len];
		m_BufferSize=len;
	}
	
	AES_set_decrypt_key((const unsigned char*)m_currentkey.c_str(),m_currentkey.size()*8,&aeskey);
	AES_cbc_encrypt((const unsigned char*)fragstr.c_str(),(unsigned char*)m_DecryptBuffer,len,&aeskey,(unsigned char*)m_currentiv.c_str(),AES_DECRYPT);
	fragstr.clear();
	fragstr.append(m_DecryptBuffer,len);
	return 0;
}

bool CHLSPaser::IsLiveStream()
{
	return m_bIsLive;
}

bool CHLSPaser::IsEncrypt()
{
	return m_IsEncrypt;
}

int CHLSPaser::SetBuffSize(int size)
{
	if(m_DecryptBuffer!=NULL)
	{
		delete m_DecryptBuffer;
	}
	m_DecryptBuffer=new char[size];
	return 0;
}