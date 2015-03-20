#!/usr/bin/env python
#coding:utf-8

import sys


def main():
    # Files were made with find -l | sed "s|^.||" > foobar
    filea = "/home/ubuntu/Desktop/ubuntu1004"
    a = open(filea).readlines()
    al = [x.replace("/usr/lib", "/lib") for x in a] # Workaround /lib vs /usr/lib
    aa = a + al
    fileb = "/home/ubuntu/Desktop/ubuntu1010"
    b = open(fileb).readlines()
    bl = [x.replace("/usr/lib", "/lib") for x in b]
    bb = b + bl
    filec = "/home/ubuntu/Desktop/fedora12"
    c = open(filec).readlines()
    cl = [x.replace("/usr/lib", "/lib") for x in c]
    cc = c + cl
    filed = "/home/ubuntu/Desktop/opensuse113"
    d = open(filed).readlines()
    dl = [x.replace("/usr/lib", "/lib") for x in d]
    dd = d + dl
    thelist = list(set(aa) & set(bb) & set(cc) & set(dd))
    thelistusrlib = [x.replace("/lib/", "/usr/lib/") for x in thelist]
    # newlist = [x.replace("\n", "") for x in thelist]
    newlist = thelist + thelistusrlib
    newlist.sort()
    # newlist.sort()
    return newlist

if __name__=='__main__':
    list = main()
    f = open("/home/ubuntu/Desktop/commonfiles.data","w")
    for line in list:
        f.write(line)
