#import "AcnhResourcePack.h"
#include "../../host/acnh_data.h"
NSString *acnh_mount_resources(NSString *folder) {
    char why[256];if(dm_acnh_data_mount(folder.fileSystemRepresentation,why,sizeof why))return nil;
    return [NSString stringWithUTF8String:why];
}
