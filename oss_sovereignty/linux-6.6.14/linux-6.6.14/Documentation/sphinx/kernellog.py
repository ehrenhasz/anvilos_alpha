import sphinx
from sphinx.util import logging
logger = logging.getLogger('kerneldoc')
def warn(app, message):
    logger.warning(message)
def verbose(app, message):
    logger.verbose(message)
def info(app, message):
    logger.info(message)
