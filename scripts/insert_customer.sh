#!/bin/bash

read -p "Customer NameID: " CUSTOMER_NAMEID
read -p "Customer Name: " CUSTOMER_NAME

psql -U pganalytics <<EOF
INSERT INTO pganalytics.pm_customer
(customer_id,name,s3bucket,schema,name_id)
VALUES
(default,'${CUSTOMER_NAME}','pganalytics_${CUSTOMER_NAMEID}','ctm_'||'${CUSTOMER_NAMEID}','${CUSTOMER_NAMEID}');
EOF

CUSTOMER_SCHEMA=ctm_${CUSTOMER_NAMEID}
PGANALYTICS_HOME="$(readlink -f "$(dirname "$0")/../")"
SQLDIR="$PGANALYTICS_HOME/data_model"
SCRIPTDIR="$PGANALYTICS_HOME/scripts"
COLETORDIR="$PGANALYTICS_HOME/coletor"
TMPDIR=/tmp/pganalytics_${CUSTOMER_NAMEID}
mkdir -p $TMPDIR

echo "Creating group ${CUSTOMER_SCHEMA}"

createuser -U postgres ${CUSTOMER_SCHEMA}

echo "Creating readonly role for customer ${1}"

createuser -U postgres ${CUSTOMER_SCHEMA}_ro --login #--pwprompt

echo "Creating importer role for customer ${1}"

createuser -U postgres ${CUSTOMER_SCHEMA}_imp --login #--pwprompt

psql -U postgres pganalytics <<EOF
CREATE SCHEMA ${CUSTOMER_SCHEMA} AUTHORIZATION pganalytics;

GRANT ctm TO ${CUSTOMER_SCHEMA};
GRANT ${CUSTOMER_SCHEMA} TO ${CUSTOMER_SCHEMA}_ro,${CUSTOMER_SCHEMA}_imp;

GRANT USAGE ON SCHEMA ${CUSTOMER_SCHEMA} TO ${CUSTOMER_SCHEMA};

ALTER DEFAULT PRIVILEGES FOR ROLE pganalytics IN SCHEMA ${CUSTOMER_SCHEMA} GRANT SELECT ON TABLES TO ${CUSTOMER_SCHEMA};
ALTER DEFAULT PRIVILEGES FOR ROLE pganalytics IN SCHEMA ${CUSTOMER_SCHEMA} GRANT EXECUTE ON FUNCTIONS TO ${CUSTOMER_SCHEMA};
ALTER DEFAULT PRIVILEGES FOR ROLE pganalytics IN SCHEMA ${CUSTOMER_SCHEMA} GRANT USAGE ON SEQUENCES TO ${CUSTOMER_SCHEMA};
ALTER DEFAULT PRIVILEGES FOR ROLE pganalytics IN SCHEMA ${CUSTOMER_SCHEMA} GRANT INSERT ON TABLES TO ${CUSTOMER_SCHEMA}_imp;
ALTER DEFAULT PRIVILEGES FOR ROLE pganalytics IN SCHEMA ${CUSTOMER_SCHEMA} GRANT UPDATE ON SEQUENCES TO ${CUSTOMER_SCHEMA}_imp;

ALTER ROLE ${CUSTOMER_SCHEMA}_ro SET search_path TO ${CUSTOMER_SCHEMA}, public;
ALTER ROLE ${CUSTOMER_SCHEMA}_imp SET search_path TO ${CUSTOMER_SCHEMA}, public;

GRANT ${CUSTOMER_SCHEMA}_imp TO ctm_importer;

REFRESH MATERIALIZED VIEW ${CUSTOMER_SCHEMA}.mvw_pglog_checkpoint;
REFRESH MATERIALIZED VIEW ${CUSTOMER_SCHEMA}.mvw_pglog_autovacuum;
EOF
cat "${SQLDIR}/template_customer.sql" | sed "s/template_customer/${CUSTOMER_SCHEMA}/g" | sed "s/^CREATE SCHEMA/--CREATE SCHEMA/g" | psql -U pganalytics 

CUSTOMER_ID=`psql -U ${CUSTOMER_SCHEMA}_imp pganalytics -Atc "SELECT customer_id FROM pm_customer_get_current()"`

moreservers=yes
while [ $moreservers = 'yes' ]
do
  read -p "Server Name: " SERVER_NAME
  read -p "Server Full Name: " SERVER_DESC
  psql -U ${CUSTOMER_SCHEMA}_imp pganalytics -c "INSERT INTO pm_server VALUES (default,${CUSTOMER_ID},'${SERVER_NAME}','${SERVER_DESC}') RETURNING *"
  moreinstances=yes
  while [ $moreinstances = 'yes' ]
  do
    read -p "Instance Port: " INSTANCE_PORT
    read -p "Instance Name: " INSTANCE_NAME
    read -p "Is Slave? [yes|no] " IS_SLAVE           
    INSTANCE_MASTER=NULL
    if [ "${IS_SLAVE}" = 'yes' ]; then
      psql -U ${CUSTOMER_SCHEMA}_imp pganalytics -c "SELECT * FROM pm_instance"
      read -p "Sender instance_id: " INSTANCE_MASTER
    fi
    psql -U ${CUSTOMER_SCHEMA}_imp pganalytics -c "INSERT INTO pm_instance VALUES (default,(SELECT server_id FROM pm_server WHERE name = '${SERVER_NAME}'),'${INSTANCE_PORT}','${INSTANCE_NAME}',${INSTANCE_MASTER})"
    read -p "More instances? [yes|no] " moreinstances
  done
  read -p "More servers? [yes|no] " moreservers
done


# AWS

aws s3api create-bucket --bucket pganalytics_${CUSTOMER_NAMEID}

aws iam create-user --user-name ${CUSTOMER_NAMEID}

aws iam create-access-key --user-name ${CUSTOMER_NAMEID} > ${TMPDIR}/.keytmp
AWS_ACCESS_KEY=`cat ${TMPDIR}/.keytmp | grep AccessKeyId | cut -b 25-44`
AWS_SECRET_KEY=`cat ${TMPDIR}/.keytmp | grep SecretAccessKey | cut -b 29-68`
rm ${TMPDIR}/.keytmp

aws iam add-user-to-group --group-name CUSTOMER --user-name ${CUSTOMER_NAMEID}


echo "{
  \"Version\": \"2012-10-17\",
  \"Statement\": [
    {
         \"Effect\":\"Allow\",
         \"Action\":[
            \"s3:PutObject\"
         ],
         \"Resource\":\"arn:aws:s3:::pganalytics_${CUSTOMER_NAMEID}/*\"
    }
  ]
}" > ${TMPDIR}/${CUSTOMER_NAMEID}_rule.json

aws iam put-user-policy --policy-name PutObjectS3 --policy-document file://${TMPDIR}/${CUSTOMER_NAMEID}_rule.json --user-name ${CUSTOMER_NAMEID}

echo "
# vim filetype=conf #
customer \"${CUSTOMER_NAMEID}\"
bucket \"pganalytics_${CUSTOMER_NAMEID}\"
access_key_id \"${AWS_ACCESS_KEY}\"
secret_access_key \"${AWS_SECRET_KEY}\"
collect_dir \"/opt/pganalytics/data\"
server_name \"${SERVER_NAME}\"
server \"${SERVER_NAME}\" {
        instance \"${INSTANCE_NAME}\" {
                conninfo \"host=localhost user=postgres password=postgres port=${INSTANCE_PORT}\"
        }
}
" | tee ${TMPDIR}/pganalytics.conf

