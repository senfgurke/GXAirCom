import os
Import("env", "projenv")
from shutil import copyfile

def copy_file(*args, **kwargs):
    print("Copying file output to project directory...")
    target = str(kwargs['target'][0])
    savename = target.split(os.path.sep)[-1]   # name of environment
    platform = target.split(os.path.sep)[-2]
    print(target.split(os.path.sep)[-1])    
    print(target.split(os.path.sep)[-2])    
    print(target.split(os.path.sep)[-3])    
    savefile = 'bin/firmware_{}.bin'.format(platform)
    #print(savename)
    print(savefile)
    copyfile(target, savefile)
    print("Done.")

env.AddPostAction("$BUILD_DIR/partitions.bin", copy_file)
env.AddPostAction("$BUILD_DIR/firmware.bin", copy_file)
env.AddPostAction("$BUILD_DIR/spiffs.bin", copy_file)