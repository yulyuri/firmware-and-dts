#!/usr/bin/env python

from OpenGL.GL import *
from OpenGL.GLU import *
import pygame
from pygame.locals import *
import serial

# Set up the serial connection
ser = serial.Serial('COM7', 115200, timeout=1)

ax = ay = az = 0.0
yaw_mode = False

def resize(width, height):
    if height == 0:
        height = 1
    glViewport(0, 0, width, height)
    glMatrixMode(GL_PROJECTION)
    glLoadIdentity()
    gluPerspective(45, 1.0 * width / height, 0.1, 100.0)
    glMatrixMode(GL_MODELVIEW)
    glLoadIdentity()

def init():
    glShadeModel(GL_SMOOTH)
    glClearColor(0.0, 0.0, 0.0, 0.0)
    glClearDepth(1.0)
    glEnable(GL_DEPTH_TEST)
    glDepthFunc(GL_LEQUAL)
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST)

def drawText(position, textString):     
    font = pygame.font.SysFont("Courier", 18, True)
    textSurface = font.render(textString, True, (255,255,255,255), (0,0,0,255))     
    textData = pygame.image.tostring(textSurface, "RGBA", True)     
    glRasterPos3d(*position)     
    glDrawPixels(textSurface.get_width(), textSurface.get_height(), GL_RGBA, GL_UNSIGNED_BYTE, textData)

def draw():
    global rquad
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)
    glLoadIdentity()
    glTranslatef(0, 0.0, -7.0)

    osd_text = "pitch: " + str("{0:.2f}".format(ay)) + ", roll: " + str("{0:.2f}".format(ax))
    osd_line = osd_text + (", yaw: " + str("{0:.2f}".format(az)) if yaw_mode else "")
    drawText((-2, -2, 2), osd_line)

    if yaw_mode:
        glRotatef(az, 0.0, 1.0, 0.0)
    glRotatef(ay, 1.0, 0.0, 0.0)
    glRotatef(-1 * ax, 0.0, 0.0, 1.0)

    # === Original Colored Cuboid Body ===
    glBegin(GL_QUADS)

    glColor3f(0.0, 1.0, 0.0)  # Top - green
    glVertex3f(1.0, 0.2, -1.0)
    glVertex3f(-1.0, 0.2, -1.0)
    glVertex3f(-1.0, 0.2, 1.0)
    glVertex3f(1.0, 0.2, 1.0)

    glColor3f(1.0, 0.5, 0.0)  # Bottom - orange
    glVertex3f(1.0, -0.2, 1.0)
    glVertex3f(-1.0, -0.2, 1.0)
    glVertex3f(-1.0, -0.2, -1.0)
    glVertex3f(1.0, -0.2, -1.0)

    glColor3f(1.0, 0.0, 0.0)  # Front - red
    glVertex3f(1.0, 0.2, 1.0)
    glVertex3f(-1.0, 0.2, 1.0)
    glVertex3f(-1.0, -0.2, 1.0)
    glVertex3f(1.0, -0.2, 1.0)

    glColor3f(1.0, 1.0, 0.0)  # Back - yellow
    glVertex3f(1.0, -0.2, -1.0)
    glVertex3f(-1.0, -0.2, -1.0)
    glVertex3f(-1.0, 0.2, -1.0)
    glVertex3f(1.0, 0.2, -1.0)

    glColor3f(0.0, 0.0, 1.0)  # Left - blue
    glVertex3f(-1.0, 0.2, 1.0)
    glVertex3f(-1.0, 0.2, -1.0)
    glVertex3f(-1.0, -0.2, -1.0)
    glVertex3f(-1.0, -0.2, 1.0)

    glColor3f(1.0, 0.0, 1.0)  # Right - purple
    glVertex3f(1.0, 0.2, -1.0)
    glVertex3f(1.0, 0.2, 1.0)
    glVertex3f(1.0, -0.2, 1.0)
    glVertex3f(1.0, -0.2, -1.0)

    glEnd()

    # === Arms + Round Propellers ===
    arm_length = 2.0
    arm_height = 0.05
    prop_radius = 0.4

    def draw_arm_and_prop(x_dir, z_dir):
        # Arm - dark gray
        glColor3f(0.2, 0.2, 0.2)
        glBegin(GL_QUADS)
        glVertex3f(x_dir * 0.2, arm_height, z_dir * 0.2)
        glVertex3f(x_dir * arm_length, arm_height, z_dir * arm_length)
        glVertex3f(x_dir * arm_length, -arm_height, z_dir * arm_length)
        glVertex3f(x_dir * 0.2, -arm_height, z_dir * 0.2)
        glEnd()

        # Propeller - blue-gray cylinder
        num_segments = 30
        cx = x_dir * arm_length
        cz = z_dir * arm_length
        cy = 0.02

        glColor3f(0.5, 0.6, 0.8)
        glBegin(GL_TRIANGLE_FAN)
        glVertex3f(cx, cy, cz)
        for i in range(num_segments + 1):
            angle = 2 * 3.14159 * i / num_segments
            x = cx + prop_radius * cos(angle)
            z = cz + prop_radius * sin(angle)
            glVertex3f(x, cy, z)
        glEnd()

    from math import sin, cos
    draw_arm_and_prop(1, 1)
    draw_arm_and_prop(-1, 1)
    draw_arm_and_prop(1, -1)
    draw_arm_and_prop(-1, -1)


def read_data():
    global ax, ay, az
    ax = ay = az = 0.0

    # Read a line from the serial port
    line = ser.readline()
    angles = line.split(b", ")
    if len(angles) == 3:    
        try:
            ax = float(angles[0])
            ay = float(angles[1])
            az = float(angles[2])
        except ValueError:
            pass

def main():
    global yaw_mode

    video_flags = OPENGL | DOUBLEBUF
    
    pygame.init()
    screen = pygame.display.set_mode((640, 480), video_flags)
    pygame.display.set_caption("Press Esc to quit, z toggles yaw mode")
    resize(640, 480)
    init()
    frames = 0
    ticks = pygame.time.get_ticks()
    while 1:
        event = pygame.event.poll()
        if event.type == QUIT or (event.type == KEYDOWN and event.key == K_ESCAPE):
            pygame.quit()
            break       
        if event.type == KEYDOWN and event.key == K_z:
            yaw_mode = not yaw_mode
            ser.write(b"z")
        read_data()
        draw()
      
        pygame.display.flip()
        frames = frames + 1

    print("fps:  %d" % ((frames * 1000) / (pygame.time.get_ticks() - ticks)))
    ser.close()

if __name__ == '__main__':
    main()
