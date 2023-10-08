# Roadmap

* [ ] KeyValue Store Interface
    * [ ] Driver/API Definitions
        * [x] Read
        * [x] Write
        * [x] Insert
        * [ ] Delete
        * [x] Help
        * [x] Exit
    * [ ] Table Printer Implementation
        * [x] Implementing prototype
        * [ ] Adapting Silo's `ex_Read`
    * [ ] Query Parser Implementation
        * [ ] Json to Procedure
    * [ ] Local Attestation
    * [ ] Remote Attestation (with Azure Attestation?)
* [ ] Silo protocol
    * [x] Concurrency Control
    * [ ] Read
        * [x] Internal Read
        * [ ] Separate Read(transaction) and ex_Read(print)
    * [x] Write
        * [x] Update `body_`(string) of `Value`
    * [x] Insert
        * [x] Invoke `Masstree::insert_value`
    * [ ] Logging
        * [x] Refactoring Logging
        * [x] Create json format log
        * [ ] Encrypting
    * [ ] Delete
    * [ ] Garbage Collection
    * [ ] Range Scan
    * [ ] Preventing phantom problem
    * [ ] Recovery
* [ ] Masstree
    * [x] Understanding Masstree
    * [x] Inesrt_value
    * [x] Get_value
    * [x] Remove_value
    * [x] Garbage Collection
    * [ ] Range Scan
* [ ] Log Tampering Detection Protocol
    * [ ] Develop Detection Mechanisms
    * [ ] Create Alerting System