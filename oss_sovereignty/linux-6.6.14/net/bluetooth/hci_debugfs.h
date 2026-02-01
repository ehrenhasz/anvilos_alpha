 

#if IS_ENABLED(CONFIG_BT_DEBUGFS)

void hci_debugfs_create_common(struct hci_dev *hdev);
void hci_debugfs_create_bredr(struct hci_dev *hdev);
void hci_debugfs_create_le(struct hci_dev *hdev);
void hci_debugfs_create_conn(struct hci_conn *conn);
void hci_debugfs_create_basic(struct hci_dev *hdev);

#else

static inline void hci_debugfs_create_common(struct hci_dev *hdev)
{
}

static inline void hci_debugfs_create_bredr(struct hci_dev *hdev)
{
}

static inline void hci_debugfs_create_le(struct hci_dev *hdev)
{
}

static inline void hci_debugfs_create_conn(struct hci_conn *conn)
{
}

static inline void hci_debugfs_create_basic(struct hci_dev *hdev)
{
}

#endif
