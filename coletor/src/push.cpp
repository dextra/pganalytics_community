#include "push.h"
#include "ServerInfo.h"
#include "util/fs.h"
#include "util/time.h"
#include "LogManager.h"

#include <algorithm>
#include <fstream>
#include <cerrno>
#include <cstring>

BEGIN_APP_NAMESPACE

Push::Push()
{
	UserConfigPtr user_config = ServerInfo::instance()->currentServerConfig()->userConfig();
	aws::AWSConnectionFactory* lFactory = aws::AWSConnectionFactory::getInstance();
	lS3Rest =  lFactory->createS3Connection(user_config->accessKeyId(), user_config->secretAccessKey());
}

void Push::sendFiles(const std::vector<std::string> &files_to_send, const std::string &path)
{
	std::string aBucketName, aKey;
	ServerConfigPtr server_config = ServerInfo::instance()->currentServerConfig();
	UserConfigPtr user_config = server_config->userConfig();
	if (server_config->pushCommand().empty())
	{
		/* Send to S3 */
		aws::AWSConnectionFactory* lFactory = aws::AWSConnectionFactory::getInstance();
		aws::S3ConnectionPtr lS3Rest =  lFactory->createS3Connection(user_config->accessKeyId(), user_config->secretAccessKey());
		aws::PutResponsePtr lPut;
		forall(it, files_to_send)
		{
			const std::string &filename = (*it);
			std::string absfilename = path + DIRECTORY_SEPARATOR + filename;
			std::ifstream ifs(absfilename.c_str(), std::ios::in | std::ios::binary);
			if (!ifs)
			{
				throw std::runtime_error(std::string("Error while opening `") + absfilename + "': " + ::strerror(errno));
			}
			ifs.exceptions(std::ios::badbit);
			/* Send the file to S3 */
			DMSG("Sending " << absfilename);
			if (Util::fs::fileExtension(absfilename) == "log")
			{
				(void)lS3Rest->put(user_config->bucket(), std::string(S3_LOG_FILES_DIR) + "/" + filename, ifs, "text/plain");
				ifs.close();
			}
			else
			{
				std::stringstream message;
				/* Message log start message */
				message << Util::time::now() << " - staring push of file `" << absfilename << "'\n";
				(void)lS3Rest->put(user_config->bucket(), std::string(S3_DATA_DIR) + "/" + filename, ifs, "text/plain");
				ifs.close();
				/* Finish and send the message log */
				message << Util::time::now() << " - push finished\n";
				(void)lS3Rest->put(user_config->bucket(), std::string(S3_NEW_FILES_DIR) + "/" + filename, message, "text/plain");
			}
			/* If we got here, means we can remove it now */
			Util::fs::remove(absfilename);
		}
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
		/* ignore invisible files (start with ".") and ".", ".." entries */
		if (filename[0] != '.')
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

