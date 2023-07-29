#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <net/sock.h>
#include <linux/netlink.h>
#include <linux/skbuff.h>

#define ADAPTER_NAME     "ETX_I2C_ADAPTER"

extern void export_netlink_data(char* data_buf, int len);

static u32 etx_func(struct i2c_adapter *adapter)
{
	return (I2C_FUNC_I2C             |
			I2C_FUNC_SMBUS_QUICK     |
			I2C_FUNC_SMBUS_BYTE      |
			I2C_FUNC_SMBUS_BYTE_DATA |
			I2C_FUNC_SMBUS_WORD_DATA |
			I2C_FUNC_SMBUS_BLOCK_DATA);
}

static s32 etx_i2c_xfer( struct i2c_adapter *adap, struct i2c_msg *msgs,int num )
{
	int i;

	for(i = 0; i < num; i++)
	{
		int j;
		struct i2c_msg *msg_temp = &msgs[i];

		pr_info("[Count: %d] [%s]: [Addr = 0x%x] [Len = %d] [Data] = ", i, __func__, msg_temp->addr, msg_temp->len);

		for( j = 0; j < msg_temp->len; j++ )
		{
			pr_cont("[0x%02x] ", msg_temp->buf[j]);
		}
	}
	return 0;
}

char data_from_usr[3];
void send_data_to_i2c(u8 addr, u8 reg_add, u8 data)
{
	data_from_usr[0] = addr;
	data_from_usr[1] = reg_add;
	data_from_usr[2] = data;	
	pr_info("Addr_: 0x%x Command_: 0x%x Byte_: 0x%x\n", addr, reg_add, data);

}

u8 i2c_command;
u16 i2c_add;
char data_from_i2c;

static s32 etx_smbus_xfer(struct i2c_adapter *adap, 
		u16 addr,
		unsigned short flags, 
		char read_write,
		u8 command, 
		int size, 
		union i2c_smbus_data *data
		)
{
	int  msg_size,i;
	char* data_buf = kmalloc(4, GFP_KERNEL);
	
	
	data_buf[0] = 7;
	data_buf[1] = addr;
	data_buf[2] = command;
	data_buf[3] = (data->byte << 1) + read_write;
	msg_size = size;
	pr_info("SMBUS XFER\n");
	for (i = 0; i < 4; i++)
		pr_info("DATA::0x%x\n", data_buf[i]);
	pr_info("Addr: 0x%x Command: 0x%x Byte: 0x%x R/W: 0x%x\n", addr, command, data->byte, read_write);
	export_netlink_data(data_buf, 4);
	
	
	if (read_write == 1 && command == data_from_usr[1] && addr == data_from_usr[0]){

		data->byte = data_from_usr[2];
	}

	return 0;
	
}

static struct i2c_algorithm etx_i2c_algorithm = {
	.smbus_xfer     = etx_smbus_xfer,
	.master_xfer    = etx_i2c_xfer,
	.functionality  = etx_func,
};

static struct i2c_adapter etx_i2c_adapter = {
	.owner	= THIS_MODULE,
	.class	= I2C_CLASS_HWMON,//| I2C_CLASS_SPD,
	.algo	= &etx_i2c_algorithm,
	.name	= ADAPTER_NAME,
	.nr	= 25,
};

static int __init etx_driver_init(void)
{
	int ret = -1;

	ret = i2c_add_numbered_adapter(&etx_i2c_adapter);

	pr_info("Bus Driver Added!!!\n");
	return ret;
}

static void __exit etx_driver_exit(void)
{
	i2c_del_adapter(&etx_i2c_adapter);
	pr_info("Bus Driver Removed!!!\n");
}

module_init(etx_driver_init);
module_exit(etx_driver_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Pratiksha0102 <pratikshathanki2501@gmail.com>");
MODULE_DESCRIPTION("Simple I2C Bus driver");
MODULE_VERSION("1.0");
