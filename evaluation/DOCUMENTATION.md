# Detailed information about the evaluation scripts

---

1. [args_and_checks.sh](./args_and_checks.sh)

**Overview:**
This script is designed to parse command-line arguments for enabling encryption and logging options and then building the controller with the specified encryption setting.

**Requirements:**
- Linux environment
- Git installed for accessing repository information
- CMake installed for building the controller

**Usage:**
- Setup: Ensure the script is executable (`chmod +x args_and_checks.sh`) and all necessary dependencies are installed.
- Command-line Arguments: Run the script with the appropriate command-line arguments:
  - `--encryption [ON|OFF]`: Enable or disable encryption.
  - `--logging [ON|OFF]`: Enable or disable logging.
Example: 
```
./args_and_checks.sh --encryption ON --logging OFF
```
- Execution: The script builds the controller with the specified encryption setting.
- Functionality:
  - The script parses command-line arguments to set encryption and logging options.
  - It then builds the controller with the specified encryption setting using CMake.

**Notes:**
- Ensure the script is run within the project directory to correctly identify the root of the repository.
- Review and adjust the script according to your project's specific requirements.

---

2. [common.sh](./common.sh)

**Overview:**
This script is designed to facilitate the execution of experiments involving running server processes, controllers, and clients for workload testing. It supports both native execution and execution within a virtual machine (VM) environment.

**Features:**
- Server Processes: The script supports running server processes such as RocksDB and Redis, both natively and within a VM.
- Controller Execution: It enables the execution of (native & GDPR) controllers, both native and within a VM.
- Client Execution: Clients can be run concurrently to simulate workloads and gather performance metrics.
- Results Logging: Experiment results, including elapsed time and average latency, are logged for analysis.

**Usage:**
- Setup: Ensure the script is executable (`chmod +x common.sh`) and all necessary dependencies are installed.
- Configuration: Modify the script variables to specify paths, server executables, directories, and other parameters as needed.
- Execution: Run the script with appropriate parameters to conduct experiments, such as specifying workload files, database types, controller types, and the number of clients.

**Notes:**
- Ensure all necessary server executables (RocksDB, Redis) and client scripts are available and configured correctly.
- Adjust the script as per your specific experiment requirements and environment setup.
- Review the script functions and comments for detailed functionality and usage information.

---

3. [default_policy_creator.sh](./default_policy_creator.sh)

**Overview:**
This script generates JSON configuration files for client default policies based on specified parameters such as the number of purposes, total clients, user ID, and monitoring status.

**Features:**
- Custom Configuration: Users can specify the number of purposes, total clients, user ID, and enable/disable monitoring.
- JSON Generation: The script dynamically generates JSON configuration files based on the specified parameters.
- Configuration Folder: Configuration files are saved in a designated folder for easy access and management.

**Usage:**
- Setup: Ensure the script is executable (`chmod +x default_policy_creator.sh`) and run in a bash environment.
- Command Line Arguments: Use command line options to specify parameters:
  - -pur: Number of purposes (default: 64)
  - -clients: Total number of clients (default: 64)
  - -uid: User ID (default: 0)
  - -monitor: Enable/disable monitoring (default: false)
- Generated Files: JSON configuration files are saved in the specified configs folder. Each file corresponds to a client with a unique configuration.

Example:
```
./default_policy_creator.sh -pur 32 -clients 32 -uid 1 -monitor true
```
**Notes:**
- Adjust the script variables and JSON structure as per your specific requirements.
- Review the script comments for detailed functionality and usage information.

---

4. [bare_metal/automated_runner.sh](./bare_metal/automated_runner.sh)

**Overview:**
This script conducts end-to-end tests for query management with encryption and logging enabled or disabled. It covers both the native direct execution, the native and GDPR (running bare-metal) controllers with various combinations of clients, databases, and workloads.
The config for the experiments are taken from [`bare_metal/config.sh`](./bare_metal/config.sh)
**Features:**
- End-to-End Testing: Conducts comprehensive end-to-end tests for query management with different configurations.
- Controller Support: Supports both native and GDPR controllers for evaluation.
- Dynamic Configuration: Allows dynamic configuration of clients, databases, and workloads.
- Results Tracking: Tracks test results in CSV files for analysis and comparison.

**Usage:**
- Setup: Ensure the script is executable (`chmod +x automated_runner.sh`) and run in a bash environment.
- Configuration: Customize the script variables to match your test environment and requirements.
- Command Line Arguments: Use command line options to set encryption and logging options.
- Run: Execute the script and monitor the test progress.
- Results Analysis: Analyze the generated CSV files to evaluate test outcomes.

Example:
```
./automated_runner.sh
```

**Notes:**
- Ensure that all dependencies and configurations are properly set up before running the script.
- Review the script comments for detailed functionality and usage information.

---

5. [VM/run_query_mgmt.sh](./VM/run_query_mgmt.sh)

**Overview:**
This script is designed to facilitate the end-to-end testing of a GDPR-compliant data management system. It automates the setup and execution of tests with varying parameters such as the number of clients, type of database, workload, encryption settings, and logging preferences.

**Requirements:**
- Linux environment
- Bash shell
- Dependencies:
- virt-customize
- cmake

**Usage:**
- Setup: Ensure that the script is executable (`chmod +x run_query_mgmt.sh`) and all necessary dependencies are installed.
- Configuration: Modify the variables within the script to match your specific environment and requirements, such as database addresses, ports, controller details, and test combinations.
- Execution: Run the script (`./run_query_mgmt.sh --encryption ON/OFF --logging ON/OFF`) to initiate the end-to-end tests according to the configured parameters.

**Functionality:**
- The script automates the setup of the controller environment, compiling the controller with encryption options if specified.
- It runs tests with both native and GDPR controllers, iterating over combinations of clients, databases, and workloads.
- For GDPR controller tests, it generates client configurations based on specified parameters and copies them into the controller image.
- Test results are recorded in CSV files, allowing for easy analysis and comparison.
- A summary of test results is printed at the end of execution.

**Notes:**
- Ensure that necessary permissions are granted for file operations and script execution.
- Verify network connectivity and accessibility of database servers before running tests.
- Review and tailor the script according to specific use cases and requirements.

---

6. [VM/VM_controller.expect](./VM/VM_controller.expect) 

**Overview:**
This Expect script automates the deployment and execution of a controller virtual machine (VM) for a GDPR-compliant data management system. It launches the VM, configures the controller, and initiates the appropriate controller type (native or GDPR) based on user-defined parameters.

**Requirements:**
- Linux environment
- Expect package installed (expect)
- Necessary permissions to execute commands with sudo

**Usage:**
- Setup: Ensure the script is executable (`chmod +x VM_controller.expect`) and Expect is installed.
- Configuration: Modify the script variables according to your environment and requirements, such as VM settings, database details, controller type, and output file path.
- Execution: Run the script with appropriate arguments:
```
./script_name <controller_type> <cores> <memory> <db_type> <db_address> <controller_address> <controller_port> <output_file> <gdpr_config> <gdpr_log_path>
```
to deploy the VM and start the controller.
- Functionality:
  - The script launches a VM using QEMU and enters the necessary login credentials.
  - It navigates to the controller directory and starts either the native or GDPR controller based on user input.
  - For the GDPR controller, it creates a log directory and configures logging.
  - Output from the controller is redirected to a specified file for analysis.

**Notes:**
- Ensure proper network connectivity and accessibility of resources (e.g., database) before running the script.
- Review and customize the script according to your specific use case and environment.
- Take necessary security precautions, especially when handling sensitive data or privileged operations.

---

7. [VM/VM_server.expect](./VM/VM_server.expect) 

**Overview:**
This Expect script automates the deployment and execution of a controller virtual machine (VM) for a GDPR-compliant data management system. It launches the VM, configures the controller, and starts either the native or GDPR controller based on user-defined parameters.

**Requirements:**
- Linux environment
- Expect package installed (expect)
- Permissions to execute commands with sudo

**Usage:**
- Setup: Ensure the script is executable (`chmod +x VM_server.expect`) and Expect is installed.
- Configuration: Modify the script variables according to your environment and requirements, such as VM settings, database details, controller type, and output file path.
- Execution: Run the script with appropriate arguments:
```
./script_name <controller_type> <cores> <memory> <db_type> <db_address> <controller_address> <controller_port> <output_file> <gdpr_config> <gdpr_log_path>
```
to deploy the VM and start the controller.
- Functionality:
  - The script launches a VM using QEMU and enters the necessary login credentials.
  - It navigates to the controller directory and starts either the native or GDPR controller based on user input.
  - For the GDPR controller, it creates a log directory and configures logging.
  - Output from the controller is redirected to a specified file for analysis.

**Notes:**
- Ensure proper network connectivity and accessibility of resources (e.g., database) before running the script.
- Review and customize the script according to your specific use case and environment.
- Take necessary security precautions, especially when handling sensitive data or privileged operations.