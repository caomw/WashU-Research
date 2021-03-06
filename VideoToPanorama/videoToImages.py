#uses ffmpeg to create a series of stills from a video
#Also will create the folder for the images to live in if it doesn't
#already exist
#fist arg is path to the videofile and filename
#second arg is the path to the output folder
#third arg is the FPS



import subprocess, sys, os

args = sys.argv

if len(args) != 4 and len(args) !=3 :
	print("Usage : python videoToImages.py <input_video> <output_folder/>")
	print("OR Usage :python videoToImages.py <input_video> <output_folder/> <frame_rate>")
	sys.exit();
subprocess.call(['rm','-rf', args[2]])
subprocess.call(['mkdir', args[2]])
output = args[2] + 'image-%5d.png'
if len(args) == 4 :
	subprocess.call(['ffmpeg', '-i', args[1], '-r', args[3], output])

else:
	subprocess.call(['ffmpeg', '-i', args[1], output])