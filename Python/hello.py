import json
import argparse

# Instantiate the parser
parser = argparse.ArgumentParser(description='Optional app description')

# Required positional argument
parser.add_argument('out_arg', type=str,
                    help='A required string positional argument')
                    

args = parser.parse_args()

print("Argument values:")
print(args.out_arg)
#print(args.opt_pos_arg)
#print(args.opt_arg)
#print(args.switch)

data = {'name': 'John', 'age': 30}

with open(args.out_arg, 'w') as f:
    json.dump(data, f)

print("file written.")

