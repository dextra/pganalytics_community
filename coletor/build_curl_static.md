Just a fast documentation of a PoC (that worked) to build curl static in my system: Linux debian 2.6.32-5-686 #1 SMP Tue May 13 16:33:32 UTC 2014 i686 GNU/Linux

Based on: http://curl.haxx.se/mail/archive-2003-03/0115.html

Download curl, go to the source and:

	$ wget wget http://curl.haxx.se/download/curl-7.41.0.tar.gz
	$ tar xvf curl-7.41.0.tar.gz
	$ cd curl-7.41.0/
	$ ./configure --disable-shared --enable-static --disable-ldaps --disable-ldap --disable-sspi --without-libssh2 --disable-rtsp
	$ make
	$ rm src/curl
	$ cd src/
	$ ../libtool  --tag=CC   --mode=link gcc  -O2 -Wno-system-headers   -all-static  -o curl curl-tool_binmode.o curl-tool_bname.o curl-tool_cb_dbg.o curl-tool_cb_hdr.o curl-tool_cb_prg.o curl-tool_cb_rea.o curl-tool_cb_see.o curl-tool_cb_wrt.o curl-tool_cfgable.o curl-tool_convert.o curl-tool_dirhie.o curl-tool_doswin.o curl-tool_easysrc.o curl-tool_formparse.o curl-tool_getparam.o curl-tool_getpass.o curl-tool_help.o curl-tool_helpers.o curl-tool_homedir.o curl-tool_hugehelp.o curl-tool_libinfo.o curl-tool_main.o curl-tool_metalink.o curl-tool_mfiles.o curl-tool_msgs.o curl-tool_operate.o curl-tool_operhlp.o curl-tool_panykey.o curl-tool_paramhlp.o curl-tool_parsecfg.o curl-tool_strdup.o curl-tool_setopt.o curl-tool_sleep.o curl-tool_urlglob.o curl-tool_util.o curl-tool_vms.o curl-tool_writeenv.o curl-tool_writeout.o curl-tool_xattr.o ../lib/curl-strtoofft.o ../lib/curl-rawstr.o ../lib/curl-nonblock.o ../lib/curl-warnless.o  ../lib/libcurl.la  -lidn -lssl -lcrypto -lssl -lcrypto -lz -lrt -ldl

In the last line, we could just use `make LDFLAGS=-all-static`, but on the tested system (Debian 6) it misses to add `-ldl`, so the above line is a copy of failed command that `make` generated with the addition of `-ldl`.

I could send a file to S3 using the static build using the code at http://tmont.com/blargh/2014/1/uploading-to-s3-in-bash:

	file=/path/to/file/to/upload.tar.gz
	bucket=your-bucket
	resource="/${bucket}/${file}"
	contentType="application/x-compressed-tar"
	dateValue=`date -R`
	stringToSign="PUT\n\n${contentType}\n${dateValue}\n${resource}"
	s3Key=xxxxxxxxxxxxxxxxxxxx
	s3Secret=xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
	signature=`echo -en ${stringToSign} | openssl sha1 -hmac ${s3Secret} -binary | base64`
	curl -X PUT -T "${file}" \
	  -H "Host: ${bucket}.s3.amazonaws.com" \
	  -H "Date: ${dateValue}" \
	  -H "Content-Type: ${contentType}" \
	  -H "Authorization: AWS ${s3Key}:${signature}" \
	  https://${bucket}.s3.amazonaws.com/${file}

This code worked perfectly, I just had to adjust the variables.

Tip: to convert the above call to a C code using libcurl (not yet tested), just add `--libcurl /path/to/example.c` to curl call.

