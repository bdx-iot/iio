
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>

#define FXAS21002_REG_DEVICE_ID 0x0C
#define FXAS21002_DECIVE_ID     0xD7

#define FXAS21002_REG_OUT_X_MSB 0x01
#define FXAS21002_REG_OUT_X_LSB 0x02

#define FXAS21002_REG_OUT_Y_MSB 0x03
#define FXAS21002_REG_OUT_Y_LSB 0x04

#define FXAS21002_REG_OUT_Z_MSB 0x05
#define FXAS21002_REG_OUT_Z_LSB 0x06

#define FXAS21002_REG_TEMP      0x12

#define FXAS21002_REG_CTRL_REG0 0x0D
#define FXAS21002_REG_CTRL_REG1 0x13 /* 0x02 active full function of gyroscope*/
#define FXAS21002_REG_CTRL_REG2 0x14
#define FXAS21002_REG_CTRL_REG3 0x15

#define FXAS21002_GYRO_CHANNEL(reg,axis) {  \
    .type = IIO_ANGL_VEL,    \
    .address = reg,     \
    .modified = 1,  \
    .channel2 = IIO_MOD_##axis, \
    .info_mask_separate = BIT(IIO_CHAN_INFO_RAW),   \
    .info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE), \
}

#define FXAS12002_TEMP_CHANNEL(reg) { \
    .type = IIO_TEMP,  \
    .address = reg,   \
    .info_mask_separate = BIT(IIO_CHAN_INFO_PROCESSED),  \
}

/*
 * At +/- 2g with 8-bit resolution, scale is computed as:
 * 62,5
 */

static const int fxas21002iio_nscale = IIO_DEGREE_TO_RAD(62500);

struct fxas21002iio_data {
    struct i2c_client *client;
};

static int fxas21002iio_enable(struct i2c_client *client, bool enable){
    int ret;

   s32 reg = i2c_smbus_read_byte_data(client, FXAS21002_REG_CTRL_REG1);

    if(reg >= 0 ){
        if(enable)
            reg =(0x02);
        else
            reg &= ~(0x02);
    }else
        return reg;

    ret = i2c_smbus_write_byte_data(client,FXAS21002_REG_CTRL_REG1,reg);

    if(ret < 0){
        return ret;
    }

    return ret;
}

static int fxas21002iio_config(struct i2c_client *client){
    int ret;

    ret = fxas21002iio_enable(client, true);
    if(ret < 0){
        return ret;
    }

    ret = i2c_smbus_write_byte_data(client, FXAS21002_REG_CTRL_REG0,0x00);
    if(ret < 0){
        return ret;
    }

    dev_info(&client->dev, "FXAS21002 configured!\n");

    return ret;

}

static int fxas21002iio_read_raw(struct iio_dev *indio_dev, struct iio_chan_spec const *channel,
                                                                int *val, int *val2, long mask){
    int ret;
    struct fxas21002iio_data *data = iio_priv(indio_dev);

    switch (mask){
    case IIO_CHAN_INFO_PROCESSED:
        switch (channel->type){
        case  IIO_TEMP:
            ret = i2c_smbus_read_byte_data(data->client, channel->address);
            if(ret < 0){
                return  ret;
            }

            *val = ret;

            return IIO_VAL_INT;
        default:
            return -EINVAL;
        }
    case  IIO_CHAN_INFO_RAW:
        switch (channel->type){
        case  IIO_ANGL_VEL:
            ret = i2c_smbus_read_byte_data(data->client, channel->address);
            if(ret < 0){
                return ret;
            }
            *val = sign_extend32(ret, 7);
            return IIO_VAL_INT;
        default:
            return -EINVAL;
        }
      case IIO_CHAN_INFO_SCALE:
        switch (channel->type) {
        case IIO_ANGL_VEL:
            *val = 0;
            *val2 = fxas21002iio_nscale;
            return IIO_VAL_INT_PLUS_MICRO;
        default:
            return -EINVAL;     /* Invalid argument */
        }
    default:
        return -EINVAL;
    }
    return 0;
}

static const struct iio_chan_spec fxas21002iio_channels[] = {

    FXAS21002_GYRO_CHANNEL(FXAS21002_REG_OUT_X_MSB, X),
    FXAS21002_GYRO_CHANNEL(FXAS21002_REG_OUT_Y_MSB, Y),
    FXAS21002_GYRO_CHANNEL(FXAS21002_REG_OUT_Z_MSB, Z),
    FXAS12002_TEMP_CHANNEL(FXAS21002_REG_TEMP),

};

static const struct i2c_device_id fxas21002iio_id[] = {
    {"fxas1002iio", 0},
    {}
};

MODULE_DEVICE_TABLE(i2c, fxas21002iio_id);

static const struct of_device_id fxas21002iio_of_match [] = {
    {.compatible = "nxp,fxas21002iio"},
    {}
};

MODULE_DEVICE_TABLE(of, fxas21002iio_of_match);


static const struct iio_info fxas21002iio_info = {
    .read_raw = fxas21002iio_read_raw,
};


static int fxas21002iio_probe(struct i2c_client *client, const struct i2c_device_id *id){

    int ret;
    struct iio_dev *indio_dev;
    struct fxas21002iio_data *data;

    if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE_DATA | I2C_FUNC_SMBUS_WORD_DATA)) {
        dev_err(&client->dev, "SMBUS Byte/Word Data not supported\n");
        return -EOPNOTSUPP;
    }

    ret = i2c_smbus_read_byte_data(client, FXAS21002_REG_DEVICE_ID);
    if (ret != FXAS21002_DECIVE_ID) {
        dev_err(&client->dev, "Invalid chip id -> %d instead of %d!\n", ret, FXAS21002_DECIVE_ID);
        return (ret < 0) ? ret : -ENODEV;
    }

    indio_dev = devm_iio_device_alloc(&client->dev,sizeof(*data));
    if(!indio_dev){
        dev_err(&client->dev, "iio allocation failed\n");
        return -ENOMEM;
    }

    data = iio_priv(indio_dev);
    data->client = client;

    i2c_set_clientdata(client, indio_dev);
    indio_dev->dev.parent = &client->dev;
    indio_dev->info = &fxas21002iio_info;
    indio_dev->name = id->name;
    indio_dev->modes = INDIO_DIRECT_MODE;
    indio_dev->channels = fxas21002iio_channels;
    indio_dev->num_channels = ARRAY_SIZE(fxas21002iio_channels);

    ret = fxas21002iio_config(client);
    if(ret < 0){
        dev_err(&client->dev, "Configuration failed\n");
        return ret;
    }

    ret = iio_device_register(indio_dev);
    if(ret < 0){
        dev_err(&client->dev, "device_register failed\n");
        return ret;
    }

    return ret;
}

static int fxas21002iio_remove(struct i2c_client *client){

    struct iio_dev *indio_dev = i2c_get_clientdata(client);
    iio_device_unregister(indio_dev);

    return fxas21002iio_enable(client, false);

}

static struct i2c_driver fxas21002iio_driver = {
    .driver = {
                .name = "fxas21002iio",
                .of_match_table = of_match_ptr(fxas21002iio_of_match),
            },
    .probe = fxas21002iio_probe,
    .remove = fxas21002iio_remove,
    .id_table = fxas21002iio_id,
};

module_i2c_driver(fxas21002iio_driver);

MODULE_AUTHOR("Joris Offouga <offougajoris@gmail.com");
MODULE_DESCRIPTION("Example iio driver for fxas21002 Gyroscope");
MODULE_LICENSE("GPL v2");
