
 

#include <soc/tegra/ivc.h>

#define TEGRA_IVC_ALIGN 64

 
enum tegra_ivc_state {
	 
	TEGRA_IVC_STATE_ESTABLISHED = 0,

	 
	TEGRA_IVC_STATE_SYNC,

	 
	TEGRA_IVC_STATE_ACK
};

 
struct tegra_ivc_header {
	union {
		struct {
			 
			u32 count;
			u32 state;
		};

		u8 pad[TEGRA_IVC_ALIGN];
	} tx;

	union {
		 
		u32 count;
		u8 pad[TEGRA_IVC_ALIGN];
	} rx;
};

#define tegra_ivc_header_read_field(hdr, field) \
	iosys_map_rd_field(hdr, 0, struct tegra_ivc_header, field)

#define tegra_ivc_header_write_field(hdr, field, value) \
	iosys_map_wr_field(hdr, 0, struct tegra_ivc_header, field, value)

static inline void tegra_ivc_invalidate(struct tegra_ivc *ivc, dma_addr_t phys)
{
	if (!ivc->peer)
		return;

	dma_sync_single_for_cpu(ivc->peer, phys, TEGRA_IVC_ALIGN,
				DMA_FROM_DEVICE);
}

static inline void tegra_ivc_flush(struct tegra_ivc *ivc, dma_addr_t phys)
{
	if (!ivc->peer)
		return;

	dma_sync_single_for_device(ivc->peer, phys, TEGRA_IVC_ALIGN,
				   DMA_TO_DEVICE);
}

static inline bool tegra_ivc_empty(struct tegra_ivc *ivc, struct iosys_map *map)
{
	 
	u32 tx = tegra_ivc_header_read_field(map, tx.count);
	u32 rx = tegra_ivc_header_read_field(map, rx.count);

	 
	if (tx - rx > ivc->num_frames)
		return true;

	return tx == rx;
}

static inline bool tegra_ivc_full(struct tegra_ivc *ivc, struct iosys_map *map)
{
	u32 tx = tegra_ivc_header_read_field(map, tx.count);
	u32 rx = tegra_ivc_header_read_field(map, rx.count);

	 
	return tx - rx >= ivc->num_frames;
}

static inline u32 tegra_ivc_available(struct tegra_ivc *ivc, struct iosys_map *map)
{
	u32 tx = tegra_ivc_header_read_field(map, tx.count);
	u32 rx = tegra_ivc_header_read_field(map, rx.count);

	 
	return tx - rx;
}

static inline void tegra_ivc_advance_tx(struct tegra_ivc *ivc)
{
	unsigned int count = tegra_ivc_header_read_field(&ivc->tx.map, tx.count);

	tegra_ivc_header_write_field(&ivc->tx.map, tx.count, count + 1);

	if (ivc->tx.position == ivc->num_frames - 1)
		ivc->tx.position = 0;
	else
		ivc->tx.position++;
}

static inline void tegra_ivc_advance_rx(struct tegra_ivc *ivc)
{
	unsigned int count = tegra_ivc_header_read_field(&ivc->rx.map, rx.count);

	tegra_ivc_header_write_field(&ivc->rx.map, rx.count, count + 1);

	if (ivc->rx.position == ivc->num_frames - 1)
		ivc->rx.position = 0;
	else
		ivc->rx.position++;
}

static inline int tegra_ivc_check_read(struct tegra_ivc *ivc)
{
	unsigned int offset = offsetof(struct tegra_ivc_header, tx.count);
	unsigned int state;

	 
	state = tegra_ivc_header_read_field(&ivc->tx.map, tx.state);
	if (state != TEGRA_IVC_STATE_ESTABLISHED)
		return -ECONNRESET;

	 
	if (!tegra_ivc_empty(ivc, &ivc->rx.map))
		return 0;

	tegra_ivc_invalidate(ivc, ivc->rx.phys + offset);

	if (tegra_ivc_empty(ivc, &ivc->rx.map))
		return -ENOSPC;

	return 0;
}

static inline int tegra_ivc_check_write(struct tegra_ivc *ivc)
{
	unsigned int offset = offsetof(struct tegra_ivc_header, rx.count);
	unsigned int state;

	state = tegra_ivc_header_read_field(&ivc->tx.map, tx.state);
	if (state != TEGRA_IVC_STATE_ESTABLISHED)
		return -ECONNRESET;

	if (!tegra_ivc_full(ivc, &ivc->tx.map))
		return 0;

	tegra_ivc_invalidate(ivc, ivc->tx.phys + offset);

	if (tegra_ivc_full(ivc, &ivc->tx.map))
		return -ENOSPC;

	return 0;
}

static int tegra_ivc_frame_virt(struct tegra_ivc *ivc, const struct iosys_map *header,
				unsigned int frame, struct iosys_map *map)
{
	size_t offset = sizeof(struct tegra_ivc_header) + ivc->frame_size * frame;

	if (WARN_ON(frame >= ivc->num_frames))
		return -EINVAL;

	*map = IOSYS_MAP_INIT_OFFSET(header, offset);

	return 0;
}

static inline dma_addr_t tegra_ivc_frame_phys(struct tegra_ivc *ivc,
					      dma_addr_t phys,
					      unsigned int frame)
{
	unsigned long offset;

	offset = sizeof(struct tegra_ivc_header) + ivc->frame_size * frame;

	return phys + offset;
}

static inline void tegra_ivc_invalidate_frame(struct tegra_ivc *ivc,
					      dma_addr_t phys,
					      unsigned int frame,
					      unsigned int offset,
					      size_t size)
{
	if (!ivc->peer || WARN_ON(frame >= ivc->num_frames))
		return;

	phys = tegra_ivc_frame_phys(ivc, phys, frame) + offset;

	dma_sync_single_for_cpu(ivc->peer, phys, size, DMA_FROM_DEVICE);
}

static inline void tegra_ivc_flush_frame(struct tegra_ivc *ivc,
					 dma_addr_t phys,
					 unsigned int frame,
					 unsigned int offset,
					 size_t size)
{
	if (!ivc->peer || WARN_ON(frame >= ivc->num_frames))
		return;

	phys = tegra_ivc_frame_phys(ivc, phys, frame) + offset;

	dma_sync_single_for_device(ivc->peer, phys, size, DMA_TO_DEVICE);
}

 
int tegra_ivc_read_get_next_frame(struct tegra_ivc *ivc, struct iosys_map *map)
{
	int err;

	if (WARN_ON(ivc == NULL))
		return -EINVAL;

	err = tegra_ivc_check_read(ivc);
	if (err < 0)
		return err;

	 
	smp_rmb();

	tegra_ivc_invalidate_frame(ivc, ivc->rx.phys, ivc->rx.position, 0,
				   ivc->frame_size);

	return tegra_ivc_frame_virt(ivc, &ivc->rx.map, ivc->rx.position, map);
}
EXPORT_SYMBOL(tegra_ivc_read_get_next_frame);

int tegra_ivc_read_advance(struct tegra_ivc *ivc)
{
	unsigned int rx = offsetof(struct tegra_ivc_header, rx.count);
	unsigned int tx = offsetof(struct tegra_ivc_header, tx.count);
	int err;

	 
	err = tegra_ivc_check_read(ivc);
	if (err < 0)
		return err;

	tegra_ivc_advance_rx(ivc);

	tegra_ivc_flush(ivc, ivc->rx.phys + rx);

	 
	smp_mb();

	 
	tegra_ivc_invalidate(ivc, ivc->rx.phys + tx);

	if (tegra_ivc_available(ivc, &ivc->rx.map) == ivc->num_frames - 1)
		ivc->notify(ivc, ivc->notify_data);

	return 0;
}
EXPORT_SYMBOL(tegra_ivc_read_advance);

 
int tegra_ivc_write_get_next_frame(struct tegra_ivc *ivc, struct iosys_map *map)
{
	int err;

	err = tegra_ivc_check_write(ivc);
	if (err < 0)
		return err;

	return tegra_ivc_frame_virt(ivc, &ivc->tx.map, ivc->tx.position, map);
}
EXPORT_SYMBOL(tegra_ivc_write_get_next_frame);

 
int tegra_ivc_write_advance(struct tegra_ivc *ivc)
{
	unsigned int tx = offsetof(struct tegra_ivc_header, tx.count);
	unsigned int rx = offsetof(struct tegra_ivc_header, rx.count);
	int err;

	err = tegra_ivc_check_write(ivc);
	if (err < 0)
		return err;

	tegra_ivc_flush_frame(ivc, ivc->tx.phys, ivc->tx.position, 0,
			      ivc->frame_size);

	 
	smp_wmb();

	tegra_ivc_advance_tx(ivc);
	tegra_ivc_flush(ivc, ivc->tx.phys + tx);

	 
	smp_mb();

	 
	tegra_ivc_invalidate(ivc, ivc->tx.phys + rx);

	if (tegra_ivc_available(ivc, &ivc->tx.map) == 1)
		ivc->notify(ivc, ivc->notify_data);

	return 0;
}
EXPORT_SYMBOL(tegra_ivc_write_advance);

void tegra_ivc_reset(struct tegra_ivc *ivc)
{
	unsigned int offset = offsetof(struct tegra_ivc_header, tx.count);

	tegra_ivc_header_write_field(&ivc->tx.map, tx.state, TEGRA_IVC_STATE_SYNC);
	tegra_ivc_flush(ivc, ivc->tx.phys + offset);
	ivc->notify(ivc, ivc->notify_data);
}
EXPORT_SYMBOL(tegra_ivc_reset);

 

int tegra_ivc_notified(struct tegra_ivc *ivc)
{
	unsigned int offset = offsetof(struct tegra_ivc_header, tx.count);
	enum tegra_ivc_state rx_state, tx_state;

	 
	tegra_ivc_invalidate(ivc, ivc->rx.phys + offset);
	rx_state = tegra_ivc_header_read_field(&ivc->rx.map, tx.state);
	tx_state = tegra_ivc_header_read_field(&ivc->tx.map, tx.state);

	if (rx_state == TEGRA_IVC_STATE_SYNC) {
		offset = offsetof(struct tegra_ivc_header, tx.count);

		 
		smp_rmb();

		 
		tegra_ivc_header_write_field(&ivc->tx.map, tx.count, 0);
		tegra_ivc_header_write_field(&ivc->rx.map, rx.count, 0);

		ivc->tx.position = 0;
		ivc->rx.position = 0;

		 
		smp_wmb();

		 
		tegra_ivc_header_write_field(&ivc->tx.map, tx.state, TEGRA_IVC_STATE_ACK);
		tegra_ivc_flush(ivc, ivc->tx.phys + offset);

		 
		ivc->notify(ivc, ivc->notify_data);

	} else if (tx_state == TEGRA_IVC_STATE_SYNC &&
		   rx_state == TEGRA_IVC_STATE_ACK) {
		offset = offsetof(struct tegra_ivc_header, tx.count);

		 
		smp_rmb();

		 
		tegra_ivc_header_write_field(&ivc->tx.map, tx.count, 0);
		tegra_ivc_header_write_field(&ivc->rx.map, rx.count, 0);

		ivc->tx.position = 0;
		ivc->rx.position = 0;

		 
		smp_wmb();

		 
		tegra_ivc_header_write_field(&ivc->tx.map, tx.state, TEGRA_IVC_STATE_ESTABLISHED);
		tegra_ivc_flush(ivc, ivc->tx.phys + offset);

		 
		ivc->notify(ivc, ivc->notify_data);

	} else if (tx_state == TEGRA_IVC_STATE_ACK) {
		offset = offsetof(struct tegra_ivc_header, tx.count);

		 
		smp_rmb();

		 
		tegra_ivc_header_write_field(&ivc->tx.map, tx.state, TEGRA_IVC_STATE_ESTABLISHED);
		tegra_ivc_flush(ivc, ivc->tx.phys + offset);

		 
		ivc->notify(ivc, ivc->notify_data);

	} else {
		 
	}

	if (tx_state != TEGRA_IVC_STATE_ESTABLISHED)
		return -EAGAIN;

	return 0;
}
EXPORT_SYMBOL(tegra_ivc_notified);

size_t tegra_ivc_align(size_t size)
{
	return ALIGN(size, TEGRA_IVC_ALIGN);
}
EXPORT_SYMBOL(tegra_ivc_align);

unsigned tegra_ivc_total_queue_size(unsigned queue_size)
{
	if (!IS_ALIGNED(queue_size, TEGRA_IVC_ALIGN)) {
		pr_err("%s: queue_size (%u) must be %u-byte aligned\n",
		       __func__, queue_size, TEGRA_IVC_ALIGN);
		return 0;
	}

	return queue_size + sizeof(struct tegra_ivc_header);
}
EXPORT_SYMBOL(tegra_ivc_total_queue_size);

static int tegra_ivc_check_params(unsigned long rx, unsigned long tx,
				  unsigned int num_frames, size_t frame_size)
{
	BUILD_BUG_ON(!IS_ALIGNED(offsetof(struct tegra_ivc_header, tx.count),
				 TEGRA_IVC_ALIGN));
	BUILD_BUG_ON(!IS_ALIGNED(offsetof(struct tegra_ivc_header, rx.count),
				 TEGRA_IVC_ALIGN));
	BUILD_BUG_ON(!IS_ALIGNED(sizeof(struct tegra_ivc_header),
				 TEGRA_IVC_ALIGN));

	if ((uint64_t)num_frames * (uint64_t)frame_size >= 0x100000000UL) {
		pr_err("num_frames * frame_size overflows\n");
		return -EINVAL;
	}

	if (!IS_ALIGNED(frame_size, TEGRA_IVC_ALIGN)) {
		pr_err("frame size not adequately aligned: %zu\n", frame_size);
		return -EINVAL;
	}

	 
	if (!IS_ALIGNED(rx, TEGRA_IVC_ALIGN)) {
		pr_err("IVC channel start not aligned: %#lx\n", rx);
		return -EINVAL;
	}

	if (!IS_ALIGNED(tx, TEGRA_IVC_ALIGN)) {
		pr_err("IVC channel start not aligned: %#lx\n", tx);
		return -EINVAL;
	}

	if (rx < tx) {
		if (rx + frame_size * num_frames > tx) {
			pr_err("queue regions overlap: %#lx + %zx > %#lx\n",
			       rx, frame_size * num_frames, tx);
			return -EINVAL;
		}
	} else {
		if (tx + frame_size * num_frames > rx) {
			pr_err("queue regions overlap: %#lx + %zx > %#lx\n",
			       tx, frame_size * num_frames, rx);
			return -EINVAL;
		}
	}

	return 0;
}

static inline void iosys_map_copy(struct iosys_map *dst, const struct iosys_map *src)
{
	*dst = *src;
}

static inline unsigned long iosys_map_get_address(const struct iosys_map *map)
{
	if (map->is_iomem)
		return (unsigned long)map->vaddr_iomem;

	return (unsigned long)map->vaddr;
}

static inline void *iosys_map_get_vaddr(const struct iosys_map *map)
{
	if (WARN_ON(map->is_iomem))
		return NULL;

	return map->vaddr;
}

int tegra_ivc_init(struct tegra_ivc *ivc, struct device *peer, const struct iosys_map *rx,
		   dma_addr_t rx_phys, const struct iosys_map *tx, dma_addr_t tx_phys,
		   unsigned int num_frames, size_t frame_size,
		   void (*notify)(struct tegra_ivc *ivc, void *data),
		   void *data)
{
	size_t queue_size;
	int err;

	if (WARN_ON(!ivc || !notify))
		return -EINVAL;

	 
	if (frame_size > INT_MAX)
		return -E2BIG;

	err = tegra_ivc_check_params(iosys_map_get_address(rx), iosys_map_get_address(tx),
				     num_frames, frame_size);
	if (err < 0)
		return err;

	queue_size = tegra_ivc_total_queue_size(num_frames * frame_size);

	if (peer) {
		ivc->rx.phys = dma_map_single(peer, iosys_map_get_vaddr(rx), queue_size,
					      DMA_BIDIRECTIONAL);
		if (dma_mapping_error(peer, ivc->rx.phys))
			return -ENOMEM;

		ivc->tx.phys = dma_map_single(peer, iosys_map_get_vaddr(tx), queue_size,
					      DMA_BIDIRECTIONAL);
		if (dma_mapping_error(peer, ivc->tx.phys)) {
			dma_unmap_single(peer, ivc->rx.phys, queue_size,
					 DMA_BIDIRECTIONAL);
			return -ENOMEM;
		}
	} else {
		ivc->rx.phys = rx_phys;
		ivc->tx.phys = tx_phys;
	}

	iosys_map_copy(&ivc->rx.map, rx);
	iosys_map_copy(&ivc->tx.map, tx);
	ivc->peer = peer;
	ivc->notify = notify;
	ivc->notify_data = data;
	ivc->frame_size = frame_size;
	ivc->num_frames = num_frames;

	 
	ivc->tx.position = 0;
	ivc->rx.position = 0;

	return 0;
}
EXPORT_SYMBOL(tegra_ivc_init);

void tegra_ivc_cleanup(struct tegra_ivc *ivc)
{
	if (ivc->peer) {
		size_t size = tegra_ivc_total_queue_size(ivc->num_frames *
							 ivc->frame_size);

		dma_unmap_single(ivc->peer, ivc->rx.phys, size,
				 DMA_BIDIRECTIONAL);
		dma_unmap_single(ivc->peer, ivc->tx.phys, size,
				 DMA_BIDIRECTIONAL);
	}
}
EXPORT_SYMBOL(tegra_ivc_cleanup);
