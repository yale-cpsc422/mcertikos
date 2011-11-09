source devenv.sh

source updatecertikoshdd.sh
#CERTIKOS_DIR=${PWD}
echo $SIMNOW_DIR
echo $CERTIKOS_DIR
cd $SIMNOW_DIR
pwd
#./simnow -f $CERTIKOS_DIR/certikos.bsd
#./simnow -f $CERTIKOS_BSD -i $CERTIKOS_HDD 
./simnow -f $CERTIKOS_BSD -e simnow_commands
cd $CERTIKOS_DIR
