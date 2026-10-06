from glob import glob

from setuptools import find_packages, setup

package_name = 'cognitive_control'

setup(
    name=package_name,
    version='0.0.0',
    packages=find_packages(exclude=['test']),
    data_files=[
        ('share/ament_index/resource_index/packages',
            ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
        ('share/' + package_name + '/launch', glob('launch/*.launch.py')),
    ],
    package_data={'': ['py.typed']},
    install_requires=['setuptools'],
    zip_safe=True,
    maintainer='a71143055',
    maintainer_email='a71143055@gmail.com',
    description='Virtual camera/tracking nodes and rosbridge GUI launch',
    license='TODO: License declaration',
    extras_require={
        'test': [
            'pytest',
        ],
    },
    entry_points={
        'console_scripts': [
            'virtual_world = cognitive_control.virtual_world:main',
            'tracker = cognitive_control.tracker:main',
            'perception = cognitive_control.perception:main',
            'device_manager = cognitive_control.device_manager:main',
            'opencr_bridge = cognitive_control.opencr_bridge:main',
        ],
    },
)
