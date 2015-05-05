passwd
# if there is edison user
deluser --remove-home edison

# vim /etc/network/interfaces
echo "\
# interfaces(5) file used by ifup(8) and ifdown(8)
auto lo
iface lo inet loopback

#auto usb0
iface usb0 inet static
    address 192.168.2.15
    netmask 255.255.255.0

auto wlan0
iface wlan0 inet dhcp
    # For WPA
    wpa-ssid SCALR SLOW
    wpa-psk [ ENTER WIFI PWD ]
    # edison network stack has horrible latency, better with this
    post-up iwconfig wlan0 power off" \
  > /etc/network/interfaces
# disable usb interface maybe
ifup wlan0

adduser --ingroup users ximus
mkdir -p /home/ximus/code

apt-get update
apt-get upgrade
apt-get install nfs-common

# make sure 192.168.1.29 is current
echo "snakebomb.local:/Users/maximeliron/code/portail-mockgate /home/ximus/code nfs auto,lookupcache=none,sync,noatime,nolock,bg,intr,actimeo=4 0 0" >> /etc/fstab
mount -a

# run /home/ximus/code/redo_mraa

# make sure enough power is provided to edison for wifi performance
# running `iwconfig wlan0 power off` helps