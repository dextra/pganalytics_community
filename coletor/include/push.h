#ifndef __PUSH_H__
#define __PUSH_H__

#include "config.h"
#include "common.h"
#include "SmartPtr.h"

#include <vector>
#include <string>

#include <libaws/aws.h>

BEGIN_APP_NAMESPACE

DECLARE_SMART_CLASS(Push);
class Push : public SmartObject
{
protected:
	aws::S3ConnectionPtr lS3Rest;
public:
	Push();
	virtual ~Push(){}
	void sendFiles(const std::vector<std::string> &files_to_send, const std::string &path = "");
	void execute();
};

END_APP_NAMESPACE

#endif // __PUSH_H__

