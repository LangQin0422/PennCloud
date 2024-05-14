# CIS 5050 final project

## Project Overview
This project involves creating a simplified cloud service platform designed to offer core functionalities similar to those of Google Apps. It provides users with web-based email and storage services, resembling Gmail and Google Drive, but with a focus on simplicity and essential features. This cloud platform is built with high availability and scalability in mind, ensuring reliable access to services and the ability to handle varying loads.

## System Architecture Overview

This section outlines the dynamic membership support in our distributed system, focusing on the frontend and backend services. Our system utilizes heartbeat mechanisms for managing frontend workers and the Paxos consensus protocol for backend membership and data rebalancing.

<p align="center">
 <img src="https://github.com/CIS5550/sp24-cis5050-T07/assets/34410439/b4bc7952-0937-439f-9857-f26a87083b5b">
</p>  


### Frontend Service: Heartbeat Mechanism

- **Frontend Servers:** A group of servers that serve as the primary interaction point for users. These servers host web servers containing the logic for various services. They are stateless by design, meaning they do not store any user data locally.

- **Load Balancer:** All user requests are first directed to a load balancer, which efficiently distributes the incoming traffic across multiple frontend servers to ensure optimal resource utilization and response times.

#### Purpose of Heartbeat Mechanism for Load Balancing

Heartbeats are periodic signals sent from frontend workers to the frontend coordinator, serving two main functions:

1. Health Checks: Indicate the operational status of the frontend workers.
2. Load Reporting: Convey current load and state metadata for optimal load balancing.

#### Process
- **Registration**: New frontend workers register with the coordinator by sending an initial registration heartbeat, which includes their address and operational metadata.
- **Health Monitoring**: Workers send regular heartbeats to maintain their registered status. Failure to receive a heartbeat within a predefined timeout results in the worker being marked as unavailable.
- **Load Balancing**: Heartbeat signals include load information, allowing the coordinator to dynamically distribute client requests across workers to optimize performance.

### Backend Services: Middleware for CRUD

The state of the system is managed by a set of backend servers that implement a key-value store (KVS) abstraction. This design enables the decoupling of service logic from data management, facilitating improved fault tolerance and easier scaling.

  - Webmail Service: Enables users to send, receive, and manage emails through a web interface.
  - Account Service: Handles user account information, authentication, and session management.
  - Web Storage Service: Allows users to store, retrieve, and manage files in the cloud, akin to a basic Google Drive functionality.

### Storage Service: Paxos Protocol over KVS

- **Key-Value Store (KVS) Group:** Clusters of KVS servers work together to store and retrieve data, with each cluster responsible for a part of data. Data are sharded with consistent hashing algorithm of *MD5* to balance the load. Within each cluster, machines employ *Paxos* algorithm to obtain agreement over operations with *Write-Ahead-Logs* (WAL) to ensure the consistency and repliability of the data, in the presence of network failures (partitions, message loss, duplication) and peer failures. In particular, the client and server side utilizes *at-least-one* and *at-most-one* semantics respectively to ensure each operation is indeed performed.

- **Data Replication and Consistency:** The system incorporates *quorum-based* replication and *sequential* consistency mode.

#### Purpose
Paxos ensures all backend nodes agree on a single configuration value despite potential system failures, providing a reliable method for managing cluster membership and data consistency.

<p align="center">
 <img src="https://github.com/CIS5550/sp24-cis5050-T07/assets/34410439/7d4810e8-e715-4be0-a57b-2b3bc2af7ec6">
</p>  

#### Dynamic Membership
- **Adding Nodes**: When adding a new node, a configuration change proposal is circulated among the existing nodes. Using Paxos, all nodes must agree on this new configuration to finalize the addition.
- **Removing Nodes**: Removal of nodes is similarly proposed and requires consensus. Concurrently, data owned by the outgoing node is rebalanced to other nodes.
- **Data Rebalancing**: Automatically triggered upon changes in node membership, ensuring even data distribution and maintaining system balance and performance.

#### Fault Tolerance
- Paxos allows the system to handle node failures and network partitions effectively, maintaining operational continuity by requiring consensus among a majority rather than all nodes.

## Prerequisites

Before you begin, ensure you have met the following requirements:

* You have OS version `Ubuntu 20.04.5 LTS`.
* You have installed the latest version of `Docker version 24.0.5, build 24.0.5-0ubuntu1~20.04.1` (for kvs clusters).
* You mush have `gcc version 9.4.0 (Ubuntu 9.4.0-1ubuntu1~20.04.1)` to support some features in our system.
  
## Setup and Installation

To install `<PennCloud>`, follow these steps:

Linux and macOS:

```bash
cd /kvs
sudo bash grpc-setup.sh
mkdir build
cd build
cmake .
make
cd ..
docker compose up -d # depending on the version, you might be docker build -t kvs:1.0 .
```
Let the docker runs and open another terminal do

```bash
mkdir build
cd build
cmake .
make
```

**Warning:** Please adjust the VM storage size to be more than 40 GB (Docker is quite heavy as we got another Ubuntu version in it).

## Running the Project

To run `<PennCloud>`, execute the included script from the project root:

```bash
cd /kvs
docker run kvs:1.0 127.0.0.1
```

This script will start the KVS cluster of 3 - 6 kvs nodes (depending on the configuration). Then, open another terminal do

```bash
bash script.sh
```

This script will start all necessary services, including 1 coordinator, 3 frontends server, 1 SMTP, and 1 POP3 server. Each will run in its own terminal window. Check one terminal call ``Frontend Coordinator`` and make sure that all tests are passed. If it does not, repeat

```bash
bash kill.sh
bash script.sh
```

untill success (Use `-v` tag for ``bash script.sh`` if you want to see the log). It is an **expected** behaviour if you start the server while kvs clusters are still initialization. 

After installation, you can use `<PennCloud>` by visiting `127.0.0.1:8000`. It will direct you to one of the hosted frontend. The admin console is under `127.0.0.1:8000/admin`

### Admin Console

<p align="center">
 <img src="https://github.com/CIS5550/sp24-cis5050-T07/assets/34410439/0c1b95c6-e9ec-4345-9ba2-ad10cb86eead">
</p>  

### Login Page

<p align="center">
 <img src="https://github.com/CIS5550/sp24-cis5050-T07/assets/34410439/6d12fab3-f6b0-4885-87ed-688c2b09fd61">
</p>

### Main Page
<p align="center">
 <img src="https://github.com/CIS5550/sp24-cis5050-T07/assets/34410439/6fac5618-5068-459d-9677-0a350a4a6393">
</p>



## Extra Features

### CICD for KVS Module

We realized a CICD pipeline utilizing **GitHub Actions** for auto-containerization of the KVS module and synchronization with *DockerHub*.

### Deployment

During the demo, all our service is deployed on AWS EC2 and KVS has a publised docker image `langqin/kvs:latest`.

### Dynamic Membership

Integrating heartbeat mechanisms and Paxos protocols significantly enhances the resilience and scalability of our distributed system. The frontend heartbeat mechanism ensures efficient load management and service availability monitoring. In contrast, the backend's use of Paxos for dynamic membership and data rebalancing safeguards data integrity and system consistency across changing network conditions and node configurations.

## Stress Test

To testify how robust and scalable our system is, we have done many stress tests on our system. The experiment code and the raw data can be found under `/stress_test`.

1. **The Influce of HTTP Server Thread Pool Size**

   In this expernment, we various the size of our HTTP thread pool to see how it might affect the overrall performance of our system.

   <p align="center">
     <img src="https://github.com/CIS5550/sp24-cis5050-T07/assets/34410439/a37c1347-453f-4ffa-b019-78692fda6c22">
   </p>   

   - **Low Thread Pool Size** (10 Threads): Exhibits the lowest throughput across the range, indicating insufficient capacity to handle more requests effectively.
   
   - **Moderate Thread Pool Size** (100 Threads): Shows improved performance over the smallest size, but throughput peaks and then declines, suggesting limitations in handling higher request volumes.
   
   - **High Thread Pool Sizes** (1,000 and 10,000 Threads): Deliver significantly better throughput, with the 1,000-thread pool showing a plateau, which suggests an optimal balance between performance and resource utilization. The 10,000-thread configuration maintains high throughput even at very high request volumes, indicating robust capacity but potentially at the cost of higher resource consumption and inefficiency due to excessive context switching.
   
   The graph underscores the importance of choosing an appropriate thread pool size to maximize server throughput while avoiding resource wastage. For our system, we believe that a thread pool size around 1,000 threads might offer the best trade-off between throughput and resource efficiency, while smaller pools might be overwhelmed and larger ones may consume excessive resources without proportional gains in throughput.

3. **How concurent requests will affect the response time**
   The following scatter plot illustrates the relationship between average response time (in milliseconds) and the number of concurent requests (requests per second) for the system.

   <p align="center">
     <img src="https://github.com/CIS5550/sp24-cis5050-T07/assets/34410439/3e922324-b93c-4acf-91e8-af7d0fad78e2">
   </p>
   From the graph we can see that 
   
   - **Inverse Relationship**: As throughput increases, average response time generally decreases, indicating more efficient processing at higher throughput levels.
   - **High Variance at Low Throughput**: Significant variability in response times at lower throughput levels, with some configurations resulting in very high response times.
   - **Stabilization of Response Time**: Beyond a throughput of 6000 requests per second, response times become more consistent and remain low, suggesting optimal system performance.
   - **Optimal Performance**: Data suggests that the system achieves optimal performance at higher throughput levels where response times are minimized.
   - **System Tuning**: Necessity for system tuning at lower throughput levels to improve response times and reduce variance.
  
  The plot underscores the importance of optimizing system configurations for higher throughput while maintaining low response times and highlights the need for further investigation into configurations that lead to high response times at lower throughputs.

4. **Distribution of Response Times Analysis**

   <p align="center">
     <img src="https://github.com/CIS5550/sp24-cis5050-T07/assets/34410439/8a461301-bd19-4350-90ee-2a60643b1cc7">
   </p>

   The histogram displays the distribution of response times for requests, highlighting efficiency and potential issues.
   **Key Observations:**
   - **Bimodal Distribution**: Indicates efficient processing for most requests with a significant peak at low response times (0-100 ms), and a smaller peak at high response times (700-800 ms), suggesting occasional delays.
   - **Primary Response Efficiency**: Most requests are handled swiftly, which is indicative of a well-optimized system under normal operations.
   - **Potential Delays**: The smaller peaks at higher response times might suggest network issues, resource contention, or processing delays affecting a subset of requests.

   **Implications:**
   - **Performance Optimization**: Focus on diagnosing and resolving the causes behind the sporadic high response times, particularly if due to network issues.
   - **Monitoring and Proactive Management**: Enhanced monitoring and alert systems are recommended to manage and mitigate performance anomalies as they occur.

   **Conclusion:**

   This histogram is instrumental for diagnosing performance across a spectrum of response times, providing insights necessary for targeted performance enhancements and informed capacity planning.


5. **Simulation of Concurrent Users vs Average Response Time**
   For this experinment, we simulate many users to have a few schedue visit on our website. This graph shows how the average response time of a system changes with the number of concurrent user requests.

   <p align="center">
     <img src="https://github.com/CIS5550/sp24-cis5050-T07/assets/34410439/9482f332-88fb-4d88-acf7-02bce049f5f0">
   </p>
  - **General Trend**: As concurrent users increase, so does the average response time, indicating rising demands on system resources.
  - **Initial Efficiency**: An initial decrease in response time suggests effective resource utilization at lower user counts.
  - **Stabilization Point**: Response time stabilizes between 2500 and 7500 users, indicating effective handling of medium loads.
  - **Increasing Delays**: Beyond 7500 users, response times climb, highlighting potential resource limitations or bottlenecks.

  **Implications:**
  - **Capacity Planning**: Identifying scalability limits is essential for ensuring the system can handle peak loads without significant performance degradation.
  - **Resource Optimization**: The increase in later stages suggests the need for enhanced resources or system optimization to manage higher loads.

  **Conclusion:**
  Understanding system performance across different load levels is crucial for maintaining efficient operations and planning for future scalability.

## Third-Party Material

1. [gRPC](https://grpc.io/): A cross-platform open source high performance remote procedure call framewor
2. [OpenSSL](https://www.openssl.org/) A toolkit for general-purpose cryptography and secure communication.
3. [Bootstrap](https://getbootstrap.com/): Create UI component in the front end
4. [Base64](https://github.com/tobiaslocker/base64/tree/master): A simple approach to convert strings from and to base64. 
5. [Abseil](https://abseil.io/): gRPC includes Abseil internelly.


## Contributing to `<PennCloud>`

To contribute to `<PennCloud>`, follow these steps:

1. Fork this repository.
2. Create a branch: `git checkout -b <branch_name>`.
3. Make your changes and commit them: `git commit -m '<commit_message>'`
4. Push to the original branch: `git push origin <PennCloud>/<location>`
5. Create the pull request.

Alternatively, see the GitHub documentation on [creating a pull request](https://help.github.com/articles/creating-a-pull-request/).

## Contributors

Thanks to the following people who have contributed to this project:

* [@Lang Qin](https://github.com/LangQin0422) langqin@seas.upenn.edu
* [@Ruikun Hao](https://github.com/haoruikun) harryhao@seas.upenn.edu
* [@Zhengyi Xiao](https://github.com/Zhengyi-Xiao) zxiao98@seas.upenn.edu
* [@Yi-Lin Cheng](https://github.com/GuodonGoGo) gocheng@seas.upenn.edu 
