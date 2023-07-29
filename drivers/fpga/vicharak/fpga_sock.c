#include <linux/module.h>
#include <net/sock.h>
#include <linux/netlink.h>
#include <linux/skbuff.h>

#define FPGA_NETLINK 30
#define UART_APP_ID  2
#define I2C_APP_ID   7

extern void send_data_to_i2c(u16 addr, u8 data, int size);
extern void send_uart_data(unsigned char data, int len);
struct sock *fpga_nl_sk = NULL;
struct nlmsghdr *nlhead;
int pid;

void export_netlink_data(char* data_buf, int len)
{

	struct sk_buff *skb_out;
	int  res;

	skb_out = nlmsg_new(len, 0);

	if(!skb_out)
	{
		printk(KERN_ERR "Failed to allocate new skb\n");
		return;
	}

	nlhead = nlmsg_put(skb_out, 0, 0, NLMSG_DONE, len, 0);

	NETLINK_CB(skb_out).dst_group = 0;                  


	strncpy(nlmsg_data(nlhead), data_buf, len);

	res = nlmsg_unicast(fpga_nl_sk, skb_out, pid); 

	if(res < 0)
		printk(KERN_INFO "Error while sending back to user\n");

}
EXPORT_SYMBOL(export_netlink_data);

static void fpga_netlink_recv_msg(struct sk_buff *skb)
{
	char data[10];
	char appid;
	nlhead = (struct nlmsghdr*)skb->data;

	pid = nlhead->nlmsg_pid;

	memcpy(data, nlmsg_data(nlhead), 4);
	appid  = data[0];
	printk(KERN_INFO "Appid: %d\n", appid);
	if (appid == UART_APP_ID){
		send_uart_data(data[1], 1);
	} else if (appid == I2C_APP_ID){
		send_data_to_i2c(data[1], data[2], data[3]);
	}
}

static int __init fpga_netlink_init(void)
{
	struct netlink_kernel_cfg cfg = {
		.input = fpga_netlink_recv_msg,
	};

	fpga_nl_sk = netlink_kernel_create(&init_net, FPGA_NETLINK, &cfg);
	if(!fpga_nl_sk)
	{
		printk(KERN_ALERT "Error creating socket.\n");
		return -10;
	}

	printk("NetLink Init OK!\n");
	return 0;
}

static void __exit fpga_netlink_exit(void)
{
	printk(KERN_INFO "exiting NetLink module\n");
	netlink_kernel_release(fpga_nl_sk);
}

module_init(fpga_netlink_init);
module_exit(fpga_netlink_exit);

MODULE_DESCRIPTION("FPGA NETLINK");
MODULE_AUTHOR("Pratiksha0102 <pratikshathanki2501@gmail.com>");
MODULE_LICENSE("GPL v2");

