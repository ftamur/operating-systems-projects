import os

dirs_list = os.listdir()
commands = ['ls', 'clear', 'grep', 'sleep', 'cd', 'open']

dirs_list.extend(commands)

if len(os.sys.argv) > 1:
    command = os.sys.argv[1]

    for dirs in dirs_list:
        if command == dirs[:len(command)]:
            print(dirs, end='')



