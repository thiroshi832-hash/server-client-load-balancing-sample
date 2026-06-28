TEMPLATE = subdirs

SUBDIRS += \
    Server \
    Client

Client.depends = Server
