#
# Makefile for the Linux TCP/IP (INET6) layer.
#

export-objs :=  ipv6_syms.o

obj-$(CONFIG_IPV6) += ipv6.o

ipv6-objs :=	af_inet6.o ip6_output.o ip6_input.o addrconf.o sit.o \
		route.o ip6_fib.o ipv6_sockglue.o ndisc.o udp.o raw.o \
		protocol.o icmp.o mcast.o reassembly.o tcp_ipv6.o \
		exthdrs.o sysctl_net_ipv6.o datagram.o proc.o \
		ip6_flowlabel.o ipv6_syms.o

#obj-$(CONFIG_IPV6_FIREWALL) += ip6_fw.o
obj-$(CONFIG_NETFILTER)	+= netfilter/

include $(TOPDIR)/Rules.make
