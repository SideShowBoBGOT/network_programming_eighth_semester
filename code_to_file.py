import os
with open('./lab_4/code.txt', 'w') as output:
    for root, dirs, files in os.walk("./lab_4"):
        for file in files:
            if file.endswith('.c') or file.endswith('.h') or file.endswith('CMakeLists.txt'):
                path = root + os.sep + file
                output.write('// ' + path + '\n\n')
                with open(path, 'r') as ff:
                    output.write(''.join(ff.readlines()))
                output.write('\n\n')
