#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>

#define LM75AD_REG_TEMP      0x00

#define LM75AD_TEMP_CHANNEL(reg) { \
    .type = IIO_TEMP,  \
    .address = reg,   \
    .info_mask_separate = BIT(IIO_CHAN_INFO_PROCESSED),  \
    .info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE), \
}

struct lm75adiio_data {
    struct i2c_client *client;
};

static int lm75adiio_read_raw(struct iio_dev *indio_dev, struct iio_chan_spec const *channel,
                                                                int *val, int *val2, long mask){
    int ret;
    struct lm75adiio_data *data = iio_priv(indio_dev);

    switch (mask){
    case IIO_CHAN_INFO_PROCESSED:
        switch (channel->type)
        {
        case  IIO_TEMP:
            ret = i2c_smbus_read_word_data(data->client, channel->address);
            if(ret < 0){
                return  ret;
            }

            *val = (int) (((ret >> 5) / 8) * 1000);

            return IIO_VAL_INT;
        default:
            return -EINVAL;
        }
    case IIO_CHAN_INFO_SCALE:
    switch (channel->type)
    {
        case IIO_TEMP:
            *val = 0;
            *val2 = 1000;
            return IIO_VAL_INT_PLUS_NANO;
        default:
            return -EINVAL;
    }
    default:
        return -EINVAL;
    }
    return 0;
}

static const struct iio_chan_spec lm75adiio_channels[] = {

    LM75AD_TEMP_CHANNEL(LM75AD_REG_TEMP),

};

static const struct i2c_device_id lm75adiio_id[] = {
    {"lm75adiio", 0},
    {}
};

MODULE_DEVICE_TABLE(i2c, lm75adiio_id);

static const struct of_device_id lm75adiio_of_match [] = {
    {.compatible = "nxp,lm75adiio"},
    {}
};

MODULE_DEVICE_TABLE(of, lm75adiio_of_match);


static const struct iio_info lm75adiio_info = {
    .read_raw = lm75adiio_read_raw,
};


static int lm75adiio_probe(struct i2c_client *client, const struct i2c_device_id *id){

    int ret;
    struct iio_dev *indio_dev;
    struct lm75adiio_data *data;

    if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE_DATA | I2C_FUNC_SMBUS_WORD_DATA)) {
        dev_err(&client->dev, "SMBUS Byte/Word Data not supported\n");
        return -EOPNOTSUPP;
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
    indio_dev->info = &lm75adiio_info;
    indio_dev->name = id->name;
    indio_dev->modes = INDIO_DIRECT_MODE;
    indio_dev->channels = lm75adiio_channels;
    indio_dev->num_channels = ARRAY_SIZE(lm75adiio_channels);

    ret = iio_device_register(indio_dev);
    if(ret < 0){
        dev_err(&client->dev, "device_register failed\n");
        return ret;
    }

    return ret;
}

static int lm75adiio_remove(struct i2c_client *client){

    struct iio_dev *indio_dev = i2c_get_clientdata(client);
    iio_device_unregister(indio_dev);

    return 0;

}

static struct i2c_driver lm75adiio_driver = {
    .driver = {
                .name = "lm75adiio",
                .of_match_table = of_match_ptr(lm75adiio_of_match),
            },
    .probe = lm75adiio_probe,
    .remove = lm75adiio_remove,
    .id_table = lm75adiio_id,
};

module_i2c_driver(lm75adiio_driver);

MODULE_AUTHOR("Joris Offouga <offougajoris@gmail.com");
MODULE_DESCRIPTION("Example iio driver for lm75ad Temperature Sensor ");
MODULE_LICENSE("GPL v2");
