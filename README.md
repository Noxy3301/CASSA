<div align="center">
<img src="./assets/cassa_logo_bounded.png" alt="logo" width="250px"></img>
</div>

# CASSA: Cloud-Adapted Secure Silo Architecture

- [What is CASSA?](#what-is-cassa)
- [Install Guide](#install-guide)
    - [Prerequisites](#prerequisites)
    - [Install and setup Intel DCAP](#install-and-setup-intel-dcap)
    - [Install Intel SGX SDK](#install-intel-sgx-sdk)
- [Build and Configurations](#build-and-configurations)
    - [Build](#build)
    - [Configuration settings](#configuration-settings)
        - [Server Application](#server-application)
        - [Client Application](#client-application)
    - [Network Configuration and PCCS Settings](#network-configuration-and-pccs-settings)
- [Technical Evaluation and Further Reading](#technical-evaluation-and-further-reading)

## What is CASSA?

CASSA leverages Intel(R) SGX to provide a data infrastructure that harmonizes confidentiality with high performance.

This system abstracts the complexities associated with SGX's often unintuitive design specifications, allowing for a more straightforward utilization of SGX to ensure data confidentiality. 
Traditionally, handling different data types necessitated altering the codebase. 
However, CASSA introduces an innovative approach, distinct from conventional TEE (Trusted Execution Environment) concepts, by abstracting the intricacies unique to SGX.
This enables a data-driven methodology, allowing developers to focus on the logic of their applications without the need to tailor their code to specific data types.

Furthermore, CASSA incorporates a mechanism for establishing secure channels using ECDSA remote attestation, enhancing security in communication between trusted and untrusted environments. This fortifies the platform's defense against man-in-the-middle attacks, ensuring that data in transit is as secure as data at rest.

With CASSA, the emphasis is on simplification and accessibility, making secure data processing available to a broader range of applications and use cases. Its architecture not only improves the usability of SGX but also expands its applicability, laying the groundwork for the next generation of secure computing.

The basical framework of these codes are heavily based on [Intel(R) Software Guard Extensions (SGX) Remote Attested TLS Sample](https://github.com/intel/linux-sgx/tree/master/SampleCode/SampleAttestedTLS) and [Silo implementation from CCBench](https://github.com/thawk105/ccbench).

## Install Guide

### Prerequisites

- Operating System Prerequisites for Intel® SGX SDK:
    - Compatible with certain Linux distributions. For full details, refer to the [Intel SGX SDK documentation](https://github.com/intel/linux-sgx?tab=readme-ov-file#prerequisites).
    - Tested on Ubuntu* 20.04 LTS Desktop 64 bits and Ubuntu* 20.04 LTS Server 64 bits.
- Ensure that you have the following required hardware with support for Intel(R) SGX (Software Guard Extensions) SGX2:
    - Processors with development codename `Gemini Lake` or `Ice Lake`
    - Later versions of Intel Xeon Scalable Processors that support Intel SGX SGX2
    - The following two Intel NUC kits, enabled for Intel SGX development, support SGX2:
        - Intel NUC Kit `NUC7CJYH` with `Intel Celeron J4005 Processor`
        - Intel NUC Kit `NUC7PJYH` with `Intel Pentium Silver J5005 Processor`

### Install and setup Intel DCAP

For detailed instructions on setting up Intel DCAP, please refer to the [Intel Software Guard Extensions Data Center Attestation Primitives Quick Install Guide](https://www.intel.com/content/www/us/en/developer/articles/guide/intel-software-guard-extensions-data-center-attestation-primitives-quick-install-guide.html).

Note: The setup has been tested on Ubuntu* 20.04 LTS Desktop 64 bits and Ubuntu* 20.04 LTS Server 64 bits.

#### 1. Subscribe to the Intel PCS Service for ECDSA Attestation to obtain your primary and secondary keys.

Access the [API Portal - Intel SGX and Intel TDX services](https://api.portal.trustedservices.intel.com/) and subscribe to the Intel PCS Service for ECDSA Attestation and acquire a pair of keys: a primary key and a secondary key. These keys are used to fetch the Provisioning Certification Key (PCK) certificates that are essential for attestation. 
Quickly access and subscribe [here](https://api.portal.trustedservices.intel.com/products#product=liv-intel-software-guard-extensions-provisioning-certification-service) (an Intel account is required).

#### 2. Install the Intel SGX DCAP Provisioning Certification Caching Service (PCCS) and configure it as a local service.

Note: The [Intel Software Guard Extensions Data Center Attestation Primitives Quick Install Guide](https://www.intel.com/content/www/us/en/developer/articles/guide/intel-software-guard-extensions-data-center-attestation-primitives-quick-install-guide.html) mentions the need for Node.js version 14, which is not included in the Ubuntu 20.04 LTS distribution. 
However, as Node.js 14.x is no longer actively supported and does not receive security or critical stability updates, we will be using Node.js version 20 for this setup. 
For the latest requirements on Node.js versions for PCCS, please refer to the official [Intel SGX DCAP documentation](https://github.com/intel/SGXDataCenterAttestationPrimitives/blob/master/QuoteGeneration/pccs/README.md).

To set up the Intel Provisioning Certification Caching Service (PCCS) on your system, follow these steps. This guide assumes you have already obtained your primary and secondary Intel PCS API keys.

##### Step 1: Importing the NodeSource GPG Key

```
$ sudo apt-get update
$ sudo apt-get install -y ca-certificates curl gnupg
$ sudo mkdir -p /etc/apt/keyrings
$ curl -fsSL https://deb.nodesource.com/gpgkey/nodesource-repo.gpg.key | sudo gpg --dearmor -o /etc/apt/keyrings/nodesource.gpg
```

##### Step 2: Setting Up the Node.js Repository

```
$ NODE_MAJOR=20
$ echo "deb [signed-by=/etc/apt/keyrings/nodesource.gpg] https://deb.nodesource.com/node_$NODE_MAJOR.x nodistro main" | sudo tee /etc/apt/sources.list.d/nodesource.list
```

##### Step 3: Installing Node.js

```
$ sudo apt-get update
$ sudo apt-get install nodejs -y
```


##### Step 4: Adding the Intel SGX Repository

```
$ sudo su
# echo 'deb [arch=amd64] https://download.01.org/intel-sgx/sgx_repo/ubuntu focal main' > /etc/apt/sources.list.d/intel-sgx.list
# wget -O - https://download.01.org/intel-sgx/sgx_repo/ubuntu/intel-sgx-deb.key | apt-key add -
# apt update
```


##### Step 5: Installing Required Build Tools

```
# apt install sqlite3 python build-essential
```

##### Step 6: Setting Up Intel SGX DCAP PCCS

```
# apt install sgx-dcap-pccs
```

Note for Step 6: During the installation of Intel SGX DCAP PCCS, you will be prompted to enter several pieces of information, such as the HTTPS listening port, whether the PCCS service should accept local connections only, and your Intel PCS API key. Please enter these details as per your requirements. If you're unsure about any step, refer to the [Intel Software Guard Extensions Data Center Attestation Primitives Quick Install Guide](https://www.intel.com/content/www/us/en/developer/articles/guide/intel-software-guard-extensions-data-center-attestation-primitives-quick-install-guide.html). for detailed explanations and guidance.


#### 3. Set up Intel SGX DCAP

```
$ sudo apt -y install libsgx-dcap-ql-dev libsgx-quote-ex-dev libsgx-enclave-common-dev libsgx-dcap-default-qpl-dev libsgx-dcap-dl libssl-dev libsgx-dcap-quote-verify libsgx-dcap-quote-verify-dev
```

#### 4. Install the sgx-pck-id-retrieval-tool to send requests to PCCS for provisioning.

Note: No need for manual DCAP driver installation on Linux kernel v5.11 and newer, as it's built-in. Our system runs on kernel 5.15.0-83-generic, confirming SGX DCAP support is automatic. Skip driver install steps for these versions.

##### Step 1: Add Intel SGX Repository and Install SGX Tools

```
$ echo 'deb [arch=amd64] https://download.01.org/intel-sgx/sgx_repo/ubuntu focal main' | sudo tee /etc/apt/sources.list.d/intel-sgx.list > /dev/null
$ wget -O - https://download.01.org/intel-sgx/sgx_repo/ubuntu/intel-sgx-deb.key | sudo apt-key add -
$ sudo apt update
$ sudo apt install sgx-pck-id-retrieval-tool
```

##### Step 2: Configure SGX PCK ID Retrieval Tool

```
$ sudo nano /opt/intel/sgx-pck-id-retrieval-tool/network_setting.conf
```

- Change the `PCCS_URL` to match your caching service’s location.
- Uncomment the `user_token` parameter, and set it to the user password you created when configuring the PCCS
- Set the `proxy_type` to fit your environment (most likely this will be “direct”)
- Ensure `USE_SECURE_CERT` is set to `“FALSE”` since we’re using a self-signed certificate for testing purposes. In a production environment, this should be set to `“TRUE”`.


##### Step 3: Add User to SGX Group and Reboot

```
$ sudo usermod -aG sgx_prv $(whoami)
$ sudo reboot
```

##### Step 4: Run PCK ID Retrieval Too

```
$ PCKIDRetrievalTool
```

If you receive a message similar to `"the data has been sent to cache server successfully and pckid_retrieval.csv has been generated successfully!"` despite a registration status error, it indicates success. The tool used your PCS API key for initial communication with the PCCS, which wasn't registered yet, hence the error. However, it successfully queried PCS and saved the results.

#### 5. Install the Intel SGX runtime libraries to enable communication with PCCS.

##### Step 1: Prepare the System

```
$ sudo apt install dkms
```

##### Step 2: Add Intel SGX Repository and Install Runtime Libraries

```
$ echo 'deb [arch=amd64] https://download.01.org/intel-sgx/sgx_repo/ubuntu focal main' | sudo tee /etc/apt/sources.list.d/intel-sgx.list > /dev/null
$ wget -O - https://download.01.org/intel-sgx/sgx_repo/ubuntu/intel-sgx-deb.key | sudo apt-key add -
$ sudo apt update
$ sudo apt-get install libsgx-urts libsgx-dcap-ql libsgx-dcap-default-qpl
```

##### Step 3: Enable ECDSA Quoting in AESM Configuration

Edit the AESM service configuration to enable ECDSA quoting by uncommenting `default quoting type = ecdsa_256`.

```
$ sudo nano /etc/aesmd.conf
```

Uncomment `default quoting type = ecdsa_256`.

##### Step 4: Restart AESM Service

Apply configuration changes by restarting the AESM service.

```
$ sudo systemctl restart aesmd
```

##### Step 5: Configure Quote Provider Library

Edit the quote provider library's configuration to communicate with the PCCS server and handle self-signed certificates.

```
$ sudo nano /etc/sgx_default_qcnl.conf
```

- Set the PCCS_URL parameter to the location of our PCCS server
- Set USE_SECURE_CERT to `“FALSE”` since we’re using a self-signed certificate for testing purposes. Again, in a production environment, this should be set to `“TRUE”`.

### Install Intel SGX SDK

For setting up the Intel SGX Software Development Kit (SDK) on your system, it's recommended to download the latest version directly from Intel's official repository. 
This ensures access to the most recent features and security enhancements.
Currently, we'll proceed with version `sgx_linux_x64_sdk_2.23.100.2`, but always check [01.org](https://download.01.org/intel-sgx/latest/linux-latest/distro/ubuntu20.04-server) for the latest release.

#### Download and Install the Intel SGX SDK

Use the following command to install the required tool to use Intel(R) SGX SDK.

```
  $ sudo apt-get install build-essential python-is-python3
```

Download and install the latest version of Intel SGX SDK.

```
$ wget https://download.01.org/intel-sgx/latest/linux-latest/distro/ubuntu20.04-server/sgx_linux_x64_sdk_2.23.100.2.bin
$ chmod 777 sgx_linux_x64_sdk_2.23.100.2.bin
$ sudo ./sgx_linux_x64_sdk_2.23.100.2.bin --prefix=/opt/intel
$ source /opt/intel/sgxsdk/environment
```

## Build and Configurations

### Build

```
$ git clone {THIS_REPO}
cd CASSA
make
```

### Configuration settings

#### Server Application

The server application requires Scalable SGX and can be started with the following command:

```
./server/app/cassa_server_host ./server/enclave/cassa_server_enclave.signed.so -port:12341 -server-in-loop
```

This command runs the server host application, specifying the signed enclave file and setting the listening port to 12341.

#### Client Application

To connect to the server application, use the following command for the client:

```
./client/cassa_client_host -server:localhost -port:12341
```

This connects the client application to the server running on localhost at port 12341.


### Network Configuration and PCCS Settings
By default, the provided commands configure both the server and client applications to communicate over localhost, facilitating local development and testing. However, to enable external access or to configure the applications for deployment, you may need to adjust the network settings, including the server address in the client application command and potentially firewall rules to allow traffic on the specified port.

Additionally, if your application utilizes Intel SGX's remote attestation features, you might need to configure the Platform Concurrency Control Services (PCCS) settings appropriately. Changing the PCCS settings can allow the application to perform attestation with Intel's services or a custom attestation service, enabling it to support external machine access securely.

## Technical Evaluation and Further Reading

For a comprehensive technical evaluation of the CASSA system, including detailed benchmarks, security assessments, and architectural insights, please refer to our published paper.
This document provides in-depth analysis and validation of the technology, offering a deeper understanding of its capabilities and performance in various environments. 

Access to the full text can be obtained through [eSilo: Making Silo Secure with SGX (CANDAR'23)](https://ieeexplore.ieee.org/document/10406173).
<!-- or its extended version [eSilo: Making Silo Secure with SGX (IJNC)](). -->
