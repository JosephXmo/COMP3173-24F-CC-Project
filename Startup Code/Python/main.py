from lexer import Lexer
import sys

# WARNING:
# - You are not allowed to use any external libraries other than the standard library
# - Please do not modify the file name of the entry file 'main.py'
# - Our autograder will test your code by runing 'python main.py <test_file>'
#   The current directory will be the same directory as the entry file
#   So please make sure your import statement is correct

if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("Usage: python main.py <test_file>")
        sys.exit(1)

    file_name = sys.argv[1]

    with open(file_name, 'r') as f:
        # read file to string
        source_code = f.read()
        # your implementation
        lexer = Lexer(source_code)
        ...
