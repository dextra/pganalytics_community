# pgAnalytics

ABOUT US

Dextra (BR) / Dexence (US) is a technology company that has a long history with the PostgreSQL world. PgAnalytics is a web based software to help DBAs and non-DBAs to better manage their PostgreSQL Instances. It is offered in two ways: the open-source version and as a Software as a Service (SaaS) tool. For more information access www.pganalytics.com.br

INSTALLATION

	PRE-REQS
		PostgreSQL Database (9.5 or higher)
		Node.JS
		NPM version 2.5.1
		NPM Modules:
			pg
			express
			cookie-parser
			serve-favicon
			morgan
			body-parser
			keypair
			nodemailer
			less
			compression

	INSTALLATION
		1. Install the pre-reqs
		2. Download the source-code (link can be found at www.pganalytics.com.br )
		3. Unpack the source-code
		4. Set up the database executing the scripts at "pganalytics_community-master/data_model/"
		5. Set up the connection string to suit your environment in "pganalytics_community-master/node_pga_2_0/pga/util/db.js":
			
			#Local connection without password:
		        var url = ':@/pganalytics?application_name=pganalytics';

		        #Local connection without password in a different database (e.g.: pga_custom, pganalytics is the default):
		        var url = ':@/pga_custom?application_name=pganalytics';

		        #Remote connection with password:
		        var url = ':my_password@my_hostname/pganalytics?application_name=pganalytics';
		6. Start the server process:
			node pganalytics_community-master/node_pga_2_0/pganalytics-server.js
		7. Access the system at http://localhost:8081/pga/login.html
		8. User/Password is demo/demo by default
