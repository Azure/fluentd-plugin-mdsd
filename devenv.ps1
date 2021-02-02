$pwd = (Get-Location).Path
$unix_pwd = "/" + (($pwd -replace "\\","/") -replace ":","")
docker build -t fluentd-plugin-mdsd-dev .
docker run -it --rm `
     -v /var/run/docker.sock:/var/run/docker.sock `
     -w /fluentd `
     -v ${pwd}:/fluentd `
     -e REPO_DIR="${unix_pwd}" `
	 fluentd-plugin-mdsd-dev
 
