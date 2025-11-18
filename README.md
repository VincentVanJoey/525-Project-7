# 525-Project-7
The group project for 525

CONNECTION SECURITY: Add secure communication support to all elements of Programming Assignment 4 (i.e., Chat Client, Chat Server, and Directory Server).
You can use any SSL/TLS library you want. The most popular ones are OpenSSL and GnuTLS.

  You must:
  
    -  Use TLS version 1.3 (modern libraries support it out-of-the-box).
    -  Use I/O such that your TLS read and write calls will not "starve" (cannot block forever) and select.
    -  Use a self-signed certificate authority (CA), and the certificates for each chat server topic and for the directory server must be signed by that CA. See this announcement for why a CA is important.

  You must NOT:
  
     - Use threads (because locking TLS connection context structures can be tricky/impossible without running into deadlocks)
     - Use timeouts (e.g., as part of a select call)

  You will need to modify the Chat Client, the Chat Server, and the Directory Server. Name your files chatClient5.c, chatServer5.c, and directoryServer5.c.

  Although it is not best practice (for future reference), you should generate your own self-signed certificates for this assignment. The Directory Server certificate must be different from Chat Server certificates. You should include "Directory Server" vs. the name of a chat room, e.g. "KSU Football", in your certificate. You must check, at connection time, to verify that you're connecting to the right entity, i.e., if you're expecting the Directory Server, you should check the certificate. Likewise, if you're expecting a Chat Server for a particular topic, you should check that topic in the certificate. Do not allow connections to servers where there is a mismatch between entity name and/or topic and/or certificate. Don't worry about client mutual authentication, i.e., don't generate certificates for Chat Clients. You may specify the chat room names (and therefore certificates) ahead of time – you don't need to dynamically generate certificates – just tell us your chat room names in the instructions.

  Again, bad practice, but for the purpose of this assignment, do not set a password for your private key. Depending on the tools you use for key generation – I recommend the command-line utility "openssl" – you may need to first generate a certificate with a password, and then remove the password. Be sure to upload keys, certificates, and/or any other files we would need to test your solution along with your code. (See announcement DO NOT EVER DO THIS EVER: How to strip a password from a key file.)
