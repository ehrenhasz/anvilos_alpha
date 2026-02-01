 
 
#ifndef __LIBMSRLISTHELPER_H__
#define __LIBMSRLISTHELPER_H__

struct i2c_client;
struct firmware;

int load_msr_list(struct i2c_client *client, char *path,
		  const struct firmware **fw);
int apply_msr_data(struct i2c_client *client, const struct firmware *fw);
void release_msr_list(struct i2c_client *client,
		      const struct firmware *fw);

#endif  
