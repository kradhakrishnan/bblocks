import time
import sys
import re
import json
import subprocess

from optparse import OptionParser

import smtplib 

#
# Config model
#

class RemoteConfig:
    def __init__(self):
	self.user = None
	self.password = None
	self.path = None

    def load(self, jdata):
	node = jdata['remote']
	self.user = node['user']
	self.password = node['password']
	self.path = node['path']

class GlobalConfig:
    def __init__(self):
	self.remote = RemoteConfig()
	self.email = None
	self.tmppath = None

    def load(self, jdata):
	node = jdata['global']
	self.remote.load(node)
	self.email = node['email']
	self.tmppath = node['tmppath']

class Env:
    def __init__(self):
	self.machine = None
	self.jobs = []

    def load(self, jdata):
	node = jdata['env']
	self.machine = node['machine']
	self.jobs = node['jobs']

class Config:
    def __init__(self):
	self.globalConfig = GlobalConfig()
	self.env = Env()

    def load(self, jdata):
	self.globalConfig.load(jdata)
	self.env.load(jdata)

#
# Helper functions
#

def SendEmail(mailAddress, subject, content):
    """
    Send email

    @emailAddress (string) Email address of the recepient
    @subject (string) Subject of the message
    @content (string) Content of the message
    """
    msgFrom = 'Flamebox@localhost.com'
    msgTo = [ mailAddress ]

    headers = [ "from: %s" % msgFrom,
	        "subject: %s" % subject,
	        "to: %s" % msgTo,
	        "mime-version: 1.0",
                "content-type: text/plain" ]

    headers = "\r\n".join(headers)

    server = smtplib.SMTP('smtp.gmail.com', 587)
    server.starttls()
    server.login('kradhakrishnan.github@gmail.com', 'githubc0d3')
    server.sendmail(msgFrom, msgTo, headers + "\r\n\r\n" + content)
    server.quit()

# .................................................................................................

def ReadFromFile(filename):
    with open(filename, 'r') as f:
	return f.read()

# .................................................................................................

def parseJSON(filename):
    """
    Parse string to construct JSON object

    :filename (string) file to be parsed
    :return JSON object
    """

    data =  re.sub(r"#(.*)", r'', ReadFromFile(filename))
    return json.loads(data)

# .................................................................................................

def runCmd(cmd, outf=sys.stdout):
    """
    Run command on the shell
    """
    outf.write("Exec : %s \n" % cmd)
    process = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, shell=True)
    outputs = process.communicate()

    for output in outputs:
	outf.write("%s" % output)

    if  process.returncode != 0:
	raise RuntimeError("Error executing command %s" % cmd)

# .................................................................................................

def runRemoteCmd(machine, password, cmd, outf=sys.stdout):
    """
    Execute ssh command on remote host
    """
    rcmd = "sshpass -p %s ssh -o StrictHostKeyChecking=no flamebox@%s '%s;exit'" % (password, machine, cmd)
    runCmd(rcmd, outf)

# .................................................................................................

def ScpTo(src, machine, password, dest, outf=sys.stdout):
    """
    scp file
    """

    rcmd = "sshpass -p %s scp %s flamebox@%s:%s" % (password, src, machine, dest)
    runCmd(rcmd, outf)

# .................................................................................................

def ScpFrom(dest, machine, password, src, recursive=False, outf=sys.stdout):
    """
    scp file
    """
    if recursive: rflag = '-r'
    else: rflag = ''

    rcmd = "sshpass -p %s scp %s flamebox@%s:%s %s" % (password, rflag, machine, dest, src)
    runCmd(rcmd, outf)

# .................................................................................................

def AnalyseAndReport(output, jobpath, jobname, jobid):
    #
    # Send email
    #
    emailAddress = 'k.radhakrishnan@icloud.com'
    subject = 'Flamebox %s : %s' % (jobid, jobname)
    data = ''.join([ReadFromFile('%s/%s/job.log' % (output, jobpath)),
		    ReadFromFile("%s/%s/%s/build/unit-test.log" % (output, jobpath, jobid))])
    SendEmail(emailAddress, subject, data)

# .................................................................................................

def runJob(jobname, machine, password, remotePath, output):
    """
    Run flamebox job on remote machine, fetch results and analyse results for errors.

    @jobname	[string]	    Name of the flamebox job
    @machine	[string]	    Host name
    @password	[string]	    Password
    @remotePath	[string]	    Path on remote host
    @output	[string]	    Path on local host
    """
    jobid = time.time()
    jobpath = "flamebox/%s" % jobid
    cmdfile = "%s/run.sh" % jobpath

    print "Running job %s @ %s : %s ==> %s" % (jobname, machine, remotePath, output)

    runCmd("mkdir -p %s/%s" % (output, jobpath))
    runCmd("touch %s/%s" % (output, cmdfile))

    cmds = [	"cd %s \n" % jobpath,
		"git clone -v https://github.com/kradhakrishnan/bblocks.cc.git \n",
		"cd bblocks.cc \n",
		"echo flamebox | sudo -S make ubuntu-setup \n",
		"~/%s/%s \n" % (jobpath, jobname)	]

    rcmd = ''.join(cmds)

    with open("%s/%s" % (output, cmdfile), 'w+') as f:
	f.write(rcmd)

    with open("%s/%s/job.log" % (output, jobpath), 'w+') as outf:
	# Setup remote machine folder
	runRemoteCmd(machine, password, "mkdir -p %s" % jobpath, outf)
	
	# Copy to remote machine
	ScpTo(jobname, machine, password, jobpath, outf)
	ScpTo("%s/%s" % (output, cmdfile), machine, password, jobpath, outf)

	# Give permission to execute
	runRemoteCmd(machine, password, "chmod +x %s" % cmdfile, outf)
	runRemoteCmd(machine, password, "chmod +x %s/%s" % (jobpath, jobname), outf)

	# Run job
	runRemoteCmd(machine, password, cmdfile, outf)

	# Copy result
	ScpFrom(jobpath, machine, password, "%s/%s" % (output, jobpath), recursive=True, outf=outf)
	runRemoteCmd(machine, password, "rm -r -f %s" % jobpath)

    AnalyseAndReport(output, jobpath, jobname, jobid)

# .................................................................................................

#
# Main
#

parser = OptionParser()

parser.add_option("--config", dest="config", help="Flamebox config file")
parser.add_option("--output", dest="output", help="Output directory")

(options, args) = parser.parse_args()

if not options.config:
    print parser.print_help()
    sys.exit(-1)

data = parseJSON(options.config)

config = Config()
config.load(data)

for job in config.env.jobs:
    print "** Running %s **" % job
    runJob(job, config.env.machine, config.globalConfig.remote.password,
	   config.globalConfig.remote.path, options.output)

