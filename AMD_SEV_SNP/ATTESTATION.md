# Steps to perform the attestion of an SEV-SNP guest:

To get an attestation report:

1. Launch an SEV-SNP VM as shown [here](https://github.com/dimstav23/GDPRuler/tree/main/AMD_SEV_SNP).

2. Inside the VM, build the host & guest SNP kernels. To do so,
```
# git clone https://github.com/AMDESE/AMDSEV.git
# cd AMDSEV
# git checkout snp-latest
# ./build.sh --package kernel
# sudo cp kvm.conf /etc/modprobe.d/
```
Tested with [this](https://github.com/AMDESE/AMDSEV/tree/b04cde73313687cbd6c21c444cfd6f7ee8d28062) commit.

3. Install the build kernel on the SNP VM:
```
# cd snp-release-{date}/linux/guest/
# sudo dpkg -i *.deb
```

4. Shutdown the VM and start it again to load the new kernel. **IMPORTANT:** SEV VMs [do not support reboots](https://github.com/AMDESE/AMDSEV/issues/157).

5. For the attestation reports the device `/dev/sev-guest` is used. You will notice it's not there.
Therefore, you now need to run `sudo modprobe sev-guest` to load the kernel module.
For more information about this undocumented phenomenon of SEV-SNP guests, you can have a look at [this thesis](https://kth.diva-portal.org/smash/get/diva2:1737821/FULLTEXT01.pdf).

6. The remote attestation is perform using the [`snpguest`](https://github.com/virtee/snpguest) tool which is linked as a submodule in the existing [`sev-snp-attestation`](https://github.com/dimstav23/sev-snp-attestation) submodule. So, run `git submodule update --init --recursive` to fetch them all. 

7. Compile the `snpguest` utility. A simple `cargo build` in its root folder should do the job. The required `rust nightly` version is already included in the `default.nix` of this project.

8. Follow the documentation of the [`sev-snp-attestation`](https://github.com/dimstav23/sev-snp-attestation) to perform the mutual attestation using self-signed certificates. The `server.py` should be executed inside the guest VM while the `client.py` resembles the side of the attester.

## Alternative solution (not tested & not maintained)

After you complete steps 1-5 above:

1. H back to the base directory and install the [`sev-guest`](https://github.com/AMDESE/sev-guest/tree/main) utility. 
Importantly, since the `sev-guest` repository is poorly maintained, you need to perform the steps listed [here](https://arkivm.github.io/sev/2022/11/25/running-amd-svsm/) to successfully compile it.
More precisely, since you need to unpack the guest kernel’s libc-dev inside sev-guest using the `dpkg -x linux-libc-dev_{version}_amd64.deb ./libc-guest` where this `.deb` package is in the `AMDSEV/snp-release-{date}/linux/guest/` directory you used earlier.
Then, perform the presented adaptations of the Makefile and run `make`.

2. To get a simple attestation report, go to sev-guest directory and run `sudo ./sev-guest-get-report report.bin`.
You can also parse the report to dump its contents `sudo ./sev-guest-parse-report report.bin`.

**Known current issues:**
1. Cert issue when building the latest guest kernel. ([SOLVED](https://github.com/AMDESE/AMDSEV/issues/156))
2. `sev-guest-get-report` fails to provide a second report. ([PENDING](https://github.com/AMDESE/sev-guest/issues/40))

**WIP:** [This](https://github.com/AMDESE/sev-guest/blob/main/docs/guest-owner-setup.md) is a guide to perform an end-to-end remote attestation procedure.

**Notes for nginx and fcgiwrap setup in a nix-shell**:

**Important!**
All the following commands should be executed from the folder where the `default.nix` file is located!

- For the `nginx` add the following in your `default.nix` configuration *let* block :
```
nginxConfigureFlags = dir: [
  "--with-threads"
  "--with-http_ssl_module"
  "--http-log-path=${dir}/nginx/access.log"
  "--error-log-path=${dir}/nginx/error.log"
  "--pid-path=${dir}/nginx/nginx.pid"
  "--http-client-body-temp-path=${dir}/nginx/client_body"
  "--http-proxy-temp-path=${dir}/nginx/proxy"
  "--http-fastcgi-temp-path=${dir}/nginx/fastcgi"
  "--http-uwsgi-temp-path=${dir}/nginx/uwsgi"
  "--http-scgi-temp-path=${dir}/nginx/scgi"
];
nginx_override = (nginx.override {
  gd = null;
  geoip = null;
  libxslt = null;
  withStream = false;
}).overrideAttrs (old: {
  configureFlags = nginxConfigureFlags "/proc/self/cwd";
});
```
and append the following in the build inputs:
```
nginx_override
fcgiwrap
```

- For the `fcgiwrap` service add the following in the server's configuration and deploy it:
```
  services.fcgiwrap = {
    enable = true;
    user = "myuser";    # Replace "myuser" with the desired username
    group = "mygroup";  # Replace "mygroup" with the desired group name
  };

  #gnutar is needed by the ssh-key-exchange.sh script used by the attestation example
  systemd.services.fcgiwrap.path = [ pkgs.gnutar ];
``` 
- Create a directory named `cgi-bin` and place inside the [scripts](https://github.com/AMDESE/sev-guest/tree/main/attestation/nginx) provided by the `sev-guest` utility. Make sure they have executable permissions.

- Create the `fcgiwrap.conf` configuration file with the following content:
```
location /cgi-bin/ { 
  # Disable gzip (it makes scripts feel slower since they have to complete
  # before getting gzipped)
  gzip off;
 
  # Set the root to the folder where the directory cgi-bin is
  # (inside this location this means that we are
  # giving access to the files under cgi-bin)
  root /home/my_user/folder_where_the_cgi_bin_directory_is;
 
  # Fastcgi socket / choose the one that is used
  fastcgi_pass  unix:/run/fcgiwrap.sock;
  #fastcgi_pass  unix:/var/run/fcgiwrap.sock;
 
  # Fastcgi parameters, include the standard ones
  # include /etc/nginx/fastcgi_params;
  include /nix/store/97r1wm31iq77lgw7lkwzb15xxyvd73ij-nginx-1.22.1/conf/fastcgi_params;

  # Adjust non standard parameters (SCRIPT_FILENAME)
  #fastcgi_param SCRIPT_FILENAME $document_root$fastcgi_script_name;
  fastcgi_param SCRIPT_FILENAME  /home/my_user/folder_where_the_cgi_bin_directory_is$fastcgi_script_name;
}
```

- Now define the `nginx` configuration file.
In our case we based our configuration on the `http.conf` file, provided [here](https://github.com/AMDESE/sev-guest/blob/main/attestation/nginx/http.conf).
```
worker_processes  1;

user my_user my_group;

events {
  worker_connections  1024;
}

http {
  default_type  application/octet-stream;

  server {
    ##
    # Replace LISTEN_ADDRESS below with the external IP address of the
    # web server, e.g.:
    # sed -i "s/LISTEN_ADDRESS/${IP}/" attestation/nginx/http.conf
    ##
    listen LISTEN_ADDRESS:80;

    server_name attestation.example.com;

    # Fast cgi support from fcgiwrap
    include /path/to/fcgiwrap.conf;
    #include /usr/share/doc/fcgiwrap/examples/nginx.conf;
  }
}
```

- Validate your `nginx` configuration:
```
nginx -c /path/to/nginx/configuration/http.conf -t
```

- If everything is fine, start your `nginx` service:
```
nginx -c /path/to/nginx/configuration/http.conf
```
*Note*: If you want to reload/stop the service, just run `nginx -s reload`/`nginx -s quit` respectively.