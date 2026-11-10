#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/io.h>

dev_t dev;
struct cdev _cdev;
struct class* clss;
struct device* dvs; 
static char* buff;
void __iomem* GPIOx;
static int GPIO_INIT_STATUS;
#define GPIO_BASE    0xfe200000

/*
 p15-0 /p15-1
*/
static int get_pin_sate(const char* args, int* pin_num, int* state){
  if(args == NULL) return 0;
  if((args[0] =='p') || (args[0] =='P')){
    int num = 0,i=1;
    for(;i<3;i++){
      if(args[i] == '-') break;
      num = (num * 10) + (args[i]-'0');
    }
    *pin_num = num;
    if(args[i] == '-'){
      i++;
      *state = ((args[i]-'0') ? 1 : 0);
      return 1;
    }
  }
  return 0;
}

static void init_gpio_pin(const int pin){
  if((GPIO_INIT_STATUS & (1 << pin)) ? 1 : 0) return;
  unsigned char offset_conf = (((pin /10) * 10) % 9) * 4;
  u32 val;
  if(GPIOx)
  {
    val = readl(GPIOx+offset_conf);
    val &=~(7 << ((pin % 10)*3));
    writel(val, GPIOx+offset_conf);

    val = readl(GPIOx+offset_conf);
    val |= (1 << ((pin % 10)*3));
    writel(val, GPIOx+offset_conf);

    GPIO_INIT_STATUS |=(1 << pin);
  }
}

static void set_gpio(const int pin, int state){
  u32 val;
  if((GPIO_INIT_STATUS & (1 << pin)) ? 1 : 0){
     if(!state)
     {
      /* set LOW */
        val = 0;
        val |=(1 << pin);
        writel(val, GPIOx+0x28);
        return;
     }
     /* set HIGH */
      val = 0;
      val |=(1 << pin);
      writel(val, GPIOx+0x1C);
  }
}

ssize_t read(struct file *, char __user * _t, size_t _s, loff_t * _off){
  return 0;
}

ssize_t write(struct file *, const char __user * _f, size_t _s, loff_t * _off){
  if(_s <4) return _s;
  int pin_num=0, state=0;
  copy_from_user(&buff[0], _f,_s);
  if(get_pin_sate(buff,&pin_num, &state))
  {
    init_gpio_pin(pin_num);
    set_gpio(pin_num, state);
  }
  return _s;
}


static struct file_operations fops = { .read = read, .write = write };
static int __init load(void)
{
  if(alloc_chrdev_region(&dev, 0, 1, "koya1_chedev") !=0) return -1;
  cdev_init(&_cdev, &fops);
  if(cdev_add(&_cdev,dev,1) !=0) return -2;
  clss = class_create(THIS_MODULE, "koya2_clss");
  dvs = device_create(clss, NULL, dev, NULL, "koya3_device");
  buff = kzalloc(100,GFP_KERNEL);
  if(!buff) return -3;
  GPIOx = ioremap(GPIO_BASE, 100);
  if(!GPIOx)
  {
    pr_info("failed to init opremap : 0%08x\n",GPIO_BASE);
    return -4;
  }
  pr_info("GPIO mapped @0x%08x\n",GPIO_BASE);
  GPIO_INIT_STATUS = 0;
  return 0;
}

static void __exit unload(void)
{
  if(GPIOx)
  {
  	iounmap(GPIOx);
  }
  if(buff){ kfree(buff); }
  device_destroy(clss, dev);
  class_destroy(clss);
  cdev_del(&_cdev);
  unregister_chrdev_region(dev,1);

}


module_init(load);
module_exit(unload);

MODULE_LICENSE("GPL");
