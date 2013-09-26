#include "geopebble.h"
#include "cJSON.h"

int debug = 1;

int main() {
    char cmd[] = "{\n\"command\" : \"cp version.h\"\n}";
    cJSON *root = cJSON_Parse(cmd);
    char *ret;
    int retlen;
    if(root != NULL) {
	ret = run_cmd(root, &retlen);
	printf("%s", ret);
	free(ret);
    }
}

    
