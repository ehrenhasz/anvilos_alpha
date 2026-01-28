import os
SITE_ROOT = "/".join(os.path.dirname(__file__).split("/")[0:-2])
TEMPLATE_DIRS = (
    "%s/templates/"
    % SITE_ROOT,  
    "%s/readthedocs/templates/" % SITE_ROOT,  
)
