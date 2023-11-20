# CASSA: Cloud-Adapted Secure Silo Architecture

### Server application
```
./server/app/cassa_server_host ./server/enclave/cassa_server_enclave.signed.so -port:12341 -server-in-loop
```

### Enclave Client application
```
./client/app/cassa_client_host ./client/enclave/cassa_client_enclave.signed.so -server:localhost -port:12341
```