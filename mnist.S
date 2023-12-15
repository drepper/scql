	.section .rodata
	.type mnist_images, @object
	.global mnist_images
mnist_images:
	.incbin "mnist-images-ubyte"
	.size mnist_images, .-mnist_images

	.type mnist_labels, @object
	.global mnist_labels
mnist_labels:
	.incbin "mnist-labels-ubyte"
	.size mnist_labels, .-mnist_labels
	.section .note.GNU-stack,"",@progbits
