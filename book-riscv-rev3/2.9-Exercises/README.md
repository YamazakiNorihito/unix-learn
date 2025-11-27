
## 起動

```bash
make qemu
```

## 動作確認

```bash
$ freemem
free memory: 133263360 bytes
$ memtest
Free memory before malloc: 133263360 bytes
Allocating 1MB...
Free memory after malloc: 132259840 bytes
Memory used: 1003520 bytes
Free memory after free: 132259840 bytes
$ freemem
free memory: 133263360 bytes
$ 
```
