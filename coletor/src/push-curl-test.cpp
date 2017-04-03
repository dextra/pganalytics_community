#include "push.h"
#include "ServerInfo.h"
#include "util/fs.h"
#include "util/time.h"
#include "LogManager.h"

#include <algorithm>
#include <fstream>
#include <cerrno>
#include <cstring>

#include <curl/curl.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/hmac.h>

BEGIN_APP_NAMESPACE

/* HMAC-SHA1, required for authentication with AWS */

std::string base64Encode(const char* aContent, size_t aContentSize, long& aBase64EncodedStringLength)
{
	char* lEncodedString;

	// initialization for base64 encoding stuff
	BIO* lBio = BIO_new(BIO_s_mem());
	BIO* lB64 = BIO_new(BIO_f_base64());
	BIO_set_flags(lB64, BIO_FLAGS_BASE64_NO_NL);
	lBio = BIO_push(lB64, lBio);

	BIO_write(lBio, aContent, aContentSize);
	BIO_flush(lBio);
	aBase64EncodedStringLength = BIO_get_mem_data(lBio, &lEncodedString);

	// ensures null termination
	std::stringstream lTmp;
	lTmp.write(lEncodedString, aBase64EncodedStringLength);

	BIO_free_all(lBio);

	return lTmp.str(); // copy
}

std::string hmac_sha1_base64(const std::string &message, const std::string &key)
{
	unsigned char result[EVP_MAX_MD_SIZE];
	unsigned int result_size;
	long base64_length;
	HMAC(EVP_sha1(), key.c_str(), key.size(),
			(const unsigned char*) message.c_str(), message.size(),
			result, &result_size);
	std::string ret((char *)result, result_size);
	return base64Encode((const char *) result, result_size, base64_length);
}

static void curl_push_test()
{
	CURLcode ret;
	CURL *hnd;
	struct curl_slist *slist1;

	slist1 = NULL;
	slist1 = curl_slist_append(slist1, "Host: your-bucket.s3.amazonaws.com");
	slist1 = curl_slist_append(slist1, "Date: Fri, 27 Feb 2015 13:34:39 -0300");
	slist1 = curl_slist_append(slist1, "Content-Type: application/x-compressed-tar");
	slist1 = curl_slist_append(slist1, "Authorization: AWS xxxxxxxxxxxxxxxxxxxx:vCwJ8Y1cHyeQR6mARgTJt8l51sQ=");

	hnd = curl_easy_init();
	curl_easy_setopt(hnd, CURLOPT_INFILESIZE_LARGE, (curl_off_t)2204);
	curl_easy_setopt(hnd, CURLOPT_URL, "https://your-bucket.s3.amazonaws.com/README.md");
	curl_easy_setopt(hnd, CURLOPT_NOPROGRESS, 1L);
	curl_easy_setopt(hnd, CURLOPT_UPLOAD, 1L);
	curl_easy_setopt(hnd, CURLOPT_USERAGENT, "curl/7.41.0");
	curl_easy_setopt(hnd, CURLOPT_HTTPHEADER, slist1);
	curl_easy_setopt(hnd, CURLOPT_MAXREDIRS, 50L);
	curl_easy_setopt(hnd, CURLOPT_CUSTOMREQUEST, "PUT");
	curl_easy_setopt(hnd, CURLOPT_TCP_KEEPALIVE, 1L);

	/* Here is a list of options the curl code used that cannot get generated
	   as source easily. You may select to either not use them or implement
	   them yourself.

	   CURLOPT_WRITEDATA set to a objectpointer
	   CURLOPT_WRITEFUNCTION set to a functionpointer
	   CURLOPT_READDATA set to a objectpointer
	   CURLOPT_READFUNCTION set to a functionpointer
	   CURLOPT_SEEKDATA set to a objectpointer
	   CURLOPT_SEEKFUNCTION set to a functionpointer
	   CURLOPT_ERRORBUFFER set to a objectpointer
	   CURLOPT_STDERR set to a objectpointer
	   CURLOPT_HEADERFUNCTION set to a functionpointer
	   CURLOPT_HEADERDATA set to a objectpointer

*/

	ret = curl_easy_perform(hnd);

	curl_easy_cleanup(hnd);
	hnd = NULL;
	curl_slist_free_all(slist1);
	slist1 = NULL;

}

Push::Push()
{
	UserConfigPtr user_config = ServerInfo::instance()->currentServerConfig()->userConfig();
	// aws::AWSConnectionFactory* lFactory = aws::AWSConnectionFactory::getInstance();
	// lS3Rest =  lFactory->createS3Connection(user_config->accessKeyId(), user_config->secretAccessKey());
}

void Push::sendFiles(const std::vector<std::string> &files_to_send, const std::string &path)
{
	std::string aBucketName, aKey;
	ServerConfigPtr server_config = ServerInfo::instance()->currentServerConfig();
	UserConfigPtr user_config = server_config->userConfig();
	if (server_config->pushCommand().empty())
	{
		std::cout << hmac_sha1_base64("Hello World", "foo") << std::endl;
		// /* Send to S3 */
		// aws::AWSConnectionFactory* lFactory = aws::AWSConnectionFactory::getInstance();
		// aws::S3ConnectionPtr lS3Rest =  lFactory->createS3Connection(user_config->accessKeyId(), user_config->secretAccessKey());
		// aws::PutResponsePtr lPut;
		// forall(it, files_to_send)
		// {
		// 	const std::string &filename = (*it);
		// 	std::string absfilename = path + DIRECTORY_SEPARATOR + filename;
		// 	std::ifstream ifs(absfilename.c_str(), std::ios::in | std::ios::binary);
		// 	if (!ifs)
		// 	{
		// 		throw std::runtime_error(std::string("Error while opening `") + absfilename + "': " + ::strerror(errno));
		// 	}
		// 	ifs.exceptions(std::ios::badbit);
		// 	/* Send the file to S3 */
		// 	DMSG("Sending " << absfilename);
		// 	if (Util::fs::fileExtension(absfilename) == "log")
		// 	{
		// 		(void)lS3Rest->put(user_config->bucket(), std::string(S3_LOG_FILES_DIR) + "/" + filename, ifs, "text/plain");
		// 		ifs.close();
		// 	}
		// 	else
		// 	{
		// 		std::stringstream message;
		// 		/* Message log start message */
		// 		message << Util::time::now() << " - staring push of file `" << absfilename << "'\n";
		// 		(void)lS3Rest->put(user_config->bucket(), std::string(S3_DATA_DIR) + "/" + filename, ifs, "text/plain");
		// 		ifs.close();
		// 		/* Finish and send the message log */
		// 		message << Util::time::now() << " - push finished\n";
		// 		(void)lS3Rest->put(user_config->bucket(), std::string(S3_NEW_FILES_DIR) + "/" + filename, message, "text/plain");
		// 	}
		// 	/* If we got here, means we can remove it now */
		// 	Util::fs::remove(absfilename);
		// }
	}
	else
	{
		/* Use configured push_command */
		forall(it, files_to_send)
		{
			const std::string &filename = (*it);
			const std::string &push_command = server_config->pushCommand();
			std::string absfilename = path + DIRECTORY_SEPARATOR + filename;
			std::string command;
			int ret;
			for (size_t i = 0; i < push_command.size(); i++)
			{
				if (push_command[i] == '%')
				{
					i++;
					switch (push_command[i])
					{
					case 'p':
						command.append(absfilename);
						break;
					case 'f':
						command.append(filename);
						break;
					case '%':
						command.append("%");
						break;
					default:
						throw std::runtime_error(std::string("Invalid push_command action '") + push_command[i] + "'");
					}
				}
				else
				{
					command += push_command[i];
				}
			}
			ret = ::system(command.c_str());
			if (ret != 0)
			{
				std::ostringstream str;
				str << "push_command failed with code " << ret << " for call: " << command;
				throw std::runtime_error(str.str());
			}
			else
			{
				Util::fs::remove(absfilename);
			}
		}
	}
}

void Push::execute()
{
	LogManagerPtr logger = LogManager::instance();
	ServerConfigPtr server_config = ServerInfo::instance()->currentServerConfig();
	Util::fs::DirReader dir_reader(server_config->collectDir() + COLLECT_DIR_NEW);
	/* Get ordered file list */
	std::vector<std::string> files_to_send; /* Use an array to send it in order */
	while (dir_reader.next() && files_to_send.size() < MAX_FILES_TO_PUSH)
	{
		std::string filename = dir_reader.entry().d_name;
		if (filename != "." && filename != "..")
		{
			files_to_send.push_back(filename);
		}
	}
	std::sort(files_to_send.begin(), files_to_send.end());
	this->sendFiles(files_to_send, server_config->collectDir() + COLLECT_DIR_NEW);
	this->sendFiles(logger->logsToPush(), server_config->collectDir() + COLLECT_DIR_LOG);
	/* Rotate current log file and resend */
	logger->rotate();
	this->sendFiles(logger->logsToPush(), server_config->collectDir() + COLLECT_DIR_LOG);
}

END_APP_NAMESPACE

