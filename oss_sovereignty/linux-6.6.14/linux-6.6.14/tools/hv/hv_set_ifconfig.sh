echo "IPV6INIT=yes" >> $1
echo "NM_CONTROLLED=no" >> $1
echo "PEERDNS=yes" >> $1
echo "ONBOOT=yes" >> $1
cp $1 /etc/sysconfig/network-scripts/
chmod 600 $2
interface=$(echo $2 | awk -F - '{ print $2 }')
filename="${2##*/}"
sed '/\[connection\]/a autoconnect=true' $2 > /etc/NetworkManager/system-connections/${filename}
/sbin/ifdown $interface 2>/dev/null
/sbin/ifup $interface 2>/dev/null
