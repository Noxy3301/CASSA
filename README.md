# CASSA: Cloud-Adapted Secure Silo Architecture

### Server application (Need Scalable SGX)
```
./server/app/cassa_server_host ./server/enclave/cassa_server_enclave.signed.so -port:12341 -server-in-loop
```

### Client application
```
./client/cassa_client_host -server:localhost -port:12341
```