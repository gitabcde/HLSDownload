#ifndef CHLSPASER_H
#define CHLSPASER_H

#include <string>
//定义mepgurl文件中的标签名称
#define HLS_STREAM_BEGIN_FLAG "#EXT-X-STREAM-INF"
#define HLS_FRAG_BEGIN_FLAG "#EXTINF"
#define HLS_NOT_LIVE_FLAG "#EXT-X-ENDLIST"
#define HLS_KEY_FLAG "#EXT-X-KEY"
#define HLS_ENCRYPTMODE_FLAG "METHOD="
#define HLS_KEY_URL_FLAG "URI="
#define HLS_IV_FLAG "IV="

//GetFragUrl的返回值
#define GFU_OK 0
#define GFU_NOTUPDATED 1
#define GFU_END -1

class CHLSPaser
{
public:
	CHLSPaser();
	~CHLSPaser();
	int SetManifestUrl(std::string url);
	int SetCookie(std::string cookie);
	int GetFragUrl(std::string& fragurl);
	int DownloadToString(std::string url,std::string& str);
	int DecryptFragData(std::string& fragstr);
	bool IsLiveStream();
	bool IsEncrypt();
	int SetBuffSize(int size);
private:
	std::string m_ManifestUrl;
	std::string m_BaseUrl;
	std::string m_Cookie;
	bool m_IsEncrypt;
	std::string::size_type m_currentkeyivpos;
	std::string::size_type m_nextkeyivpos;
	std::string m_currentkey;
	std::string m_currentiv;
	std::string m_currentencryptmode;
	std::string m_ManifestContent;
	std::string m_UpdatedManifestContent;
	std::string m_OldFragUrl;
	std::string::size_type m_CurrentFragPos;
	int m_FragDuration;
	bool m_bRelativeFragUrl;
	bool m_bIsLive;
	char* m_DecryptBuffer;
	int m_BufferSize;
};

#endif