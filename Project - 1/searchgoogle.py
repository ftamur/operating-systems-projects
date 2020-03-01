import subprocess
import sys

def install(package):
    subprocess.check_call([sys.executable, "-m", "pip", "install", package])

try:
    from googlesearch import search
except:
    install('google')
else:
    from googlesearch import search

query = input("Type query want to search: ")
  
for result in search(query, tld="co.in", num=10, stop=5, pause=2): 
    print(result) 
